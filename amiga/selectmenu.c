/*
 * Copyright 2008 - 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifdef __amigaos4__

#include <stdbool.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/popupmenu.h>
#include <reaction/reaction_macros.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "render/form.h"
#include "desktop/mouse.h"

#include "amiga/gui.h"
#include "amiga/selectmenu.h"
#include "amiga/theme.h"
#include "amiga/utf8.h"

/** Exported interface documented in selectmenu.h **/
BOOL ami_selectmenu_is_safe(void)
{
	struct Library *PopupMenuBase;
	BOOL popupmenu_lib_ok = FALSE;

	if((PopupMenuBase = OpenLibrary("popupmenu.library", 53))) {
		LOG("popupmenu.library v%d.%d", PopupMenuBase->lib_Version, PopupMenuBase->lib_Revision);
		if(LIB_IS_AT_LEAST((struct Library *)PopupMenuBase, 53, 11))
			popupmenu_lib_ok = TRUE;
		CloseLibrary(PopupMenuBase);
	}

	return popupmenu_lib_ok;
}

HOOKF(uint32, ami_popup_hook, Object *, item, APTR)
{
	uint32 itemid = 0;
	struct gui_window *gwin = hook->h_Data;

	if(GetAttr(PMIA_ID, item, &itemid)) {
		form_select_process_selection(gwin->shared->control, itemid);
	}

	return itemid;
}

void gui_create_form_select_menu(struct gui_window *g,
		struct form_control *control)
{
	struct Library *PopupMenuBase = NULL;
	struct PopupMenuIFace *IPopupMenu = NULL;
	struct Hook ctxmenuhook;
	Object *selectmenuobj;
	struct form_option *opt = form_select_get_option(control, 0);
	ULONG i = 0;

	if(ami_selectmenu_is_safe() == FALSE) return;

	if((PopupMenuBase = OpenLibrary("popupmenu.class", 0))) {
		IPopupMenu = (struct PopupMenuIFace *)GetInterface(PopupMenuBase, "main", 1, NULL);
	}

	if(IPopupMenu == NULL) return;

	ctxmenuhook.h_Entry = ami_popup_hook;
	ctxmenuhook.h_SubEntry = NULL;
	ctxmenuhook.h_Data = g;

	g->shared->control = control;

	/**\todo PMIA_Title memory leaks as we don't free the strings.
	 */

	selectmenuobj = PMMENU(ami_utf8_easy(form_control_get_name(control))),
                        PMA_MenuHandler, &ctxmenuhook, End;

	while(opt) {
		IDoMethod(selectmenuobj, PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, (ULONG)ami_utf8_easy(opt->text),
				PMIA_ID, i,
				PMIA_CheckIt, TRUE,
				PMIA_Checked, opt->selected,
				TAG_DONE),
			~0);

		opt = opt->next;
		i++;
	}

	ami_set_pointer(g->shared, GUI_POINTER_DEFAULT, false); // Clear the menu-style pointer

	IDoMethod(selectmenuobj, PM_OPEN, g->shared->win);

	/* PM_OPEN is blocking, so dispose menu immediately... */
	if(selectmenuobj) DisposeObject(selectmenuobj);

	/* ...and get rid of popupmenu.class ASAP */
	if(IPopupMenu) DropInterface((struct Interface *)IPopupMenu);
	if(PopupMenuBase) CloseLibrary(PopupMenuBase);
}

#else
#include "amiga/selectmenu.h"
void gui_create_form_select_menu(struct gui_window *g, struct form_control *control)
{
}
#endif

