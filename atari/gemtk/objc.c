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

#include <assert.h>
 #include "gemtk.h"

char *get_text(OBJECT * tree, short idx)
{
	static char p[]="";
	USERBLK *user;
	char *retval;

	switch (tree[idx].ob_type & 0x00FF) {
		case G_BUTTON:
		case G_STRING:
		case G_TITLE:
			return( tree[idx].ob_spec.free_string);
		case G_TEXT:
		case G_BOXTEXT:
		case G_FTEXT:
		case G_FBOXTEXT:
			return (tree[idx].ob_spec.tedinfo->te_ptext);
		case G_ICON:
		case G_CICON:
			return (tree[idx].ob_spec.iconblk->ib_ptext);
			break;

		default: break;
	}
	return (p);
}

static void set_text(OBJECT *obj, short idx, char * text, int len)
{
	char spare[255];

	if( len > 254 )
		len = 254;
	if( text != NULL ){
		strncpy(spare, text, 254);
	} else {
		strcpy(spare, "");
	}

	set_string(obj, idx, spare);
}

char gemtk_obj_set_str_safe(OBJECT * tree, short idx, char *txt)
{
	char spare[204];
	short type = 0;
	short maxlen = 0;
	TEDINFO *ted;


	type = (tree[idx].ob_type & 0xFF);
	if (type == G_FTEXT || type == G_FBOXTEXT) {
		TEDINFO *ted = ((TEDINFO *)get_obspec(tree, idx));
		maxlen = ted->te_txtlen+1;
		if (maxlen > 200) {
			maxlen = 200;
		}
		else if (maxlen < 0) {
			maxlen = 0;
		}
	} else {
		assert((type == G_FTEXT) || (type == G_FBOXTEXT));
	}

	snprintf(spare, maxlen, "%s", txt);
	set_string(tree, idx, spare);
}

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


void obj_mouse_sprite(OBJECT *tree, int index)
{
    MFORM mform;
    int dum;

    if ((tree[index].ob_type & 0xFF) != G_ICON)
        return;

    dum = tree[index].ob_spec.iconblk->ib_char;
    mform . mf_nplanes = 1;
    mform . mf_fg = (dum>>8)&0x0F;
    mform . mf_bg = dum>>12;
    mform . mf_xhot = 0; /* to prevent the mform to "jump" on the */
    mform . mf_yhot = 0; /* screen (zebulon rules!) */

    for( dum = 0; dum<16; dum ++) {
        mform . mf_mask[dum] = tree[index].ob_spec.iconblk->ib_pmask[dum];
        mform . mf_data[dum] = tree[index].ob_spec.iconblk->ib_pdata[dum];
    }
    graf_mouse(USER_DEF, &mform);
}
