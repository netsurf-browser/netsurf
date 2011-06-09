/*
 * Copyright 2008-9 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/popupmenu.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <reaction/reaction_macros.h>

#include "amiga/context_menu.h"
#include "amiga/clipboard.h"
#include "amiga/bitmap.h"
#include "amiga/gui.h"
#include "amiga/history_local.h"
#include "amiga/iff_dr2d.h"
#include "amiga/options.h"
#include "amiga/plugin_hack.h"
#include "amiga/theme.h"
#include "amiga/utf8.h"
#include "desktop/textinput.h"
#include "desktop/selection.h"
#include "desktop/searchweb.h"
#include "desktop/history_core.h"
#include "render/box.h"
#include "render/form.h"
#include "utils/utf8.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include <string.h>

static uint32 ami_context_menu_hook(struct Hook *hook,Object *item,APTR reserved);
static bool ami_context_menu_history(const struct history *history, int x0, int y0,
	int x1, int y1, const struct history_entry *entry, void *user_data);

uint32 ami_popup_hook(struct Hook *hook,Object *item,APTR reserved);

enum {
	CMID_SELECTFILE,
	CMID_COPYURL,
	CMID_URLOPENWIN,
	CMID_URLOPENTAB,
	CMID_SAVEURL,
	CMID_SHOWOBJ,
	CMID_COPYOBJ,
	CMID_CLIPOBJ,
	CMID_SAVEOBJ,
	CMID_SAVEIFFOBJ,
	CMID_SELALL,
	CMID_SELCLEAR,
	CMID_SELCUT,
	CMID_SELCOPY,
	CMID_SELPASTE,
	CMID_SELSEARCH,
	CMID_PLUGINCMD,
	CMSUB_OBJECT,
	CMSUB_URL,
	CMSUB_SEL,
	CMID_HISTORY,
	CMID_LAST
};

struct Library  *PopupMenuBase = NULL;
struct PopupMenuIFace *IPopupMenu = NULL;
char *ctxmenulab[CMID_LAST];

void ami_context_menu_init(void)
{
	if(PopupMenuBase = OpenLibrary("popupmenu.class",0))
	{
		IPopupMenu = (struct PopupMenuIFace *)GetInterface(PopupMenuBase,"main",1,NULL);
	}

	ctxmenulab[CMID_SELECTFILE] = ami_utf8_easy((char *)messages_get("SelectFile"));
	ctxmenulab[CMID_COPYURL] = ami_utf8_easy((char *)messages_get("CopyURL"));
	ctxmenulab[CMID_SHOWOBJ] = ami_utf8_easy((char *)messages_get("ObjShow"));
	ctxmenulab[CMID_COPYOBJ] = ami_utf8_easy((char *)messages_get("CopyURL"));
	ctxmenulab[CMID_CLIPOBJ] = ami_utf8_easy((char *)messages_get("CopyClip"));
	ctxmenulab[CMID_SAVEOBJ] = ami_utf8_easy((char *)messages_get("SaveAs"));
	ctxmenulab[CMID_SAVEIFFOBJ] = ami_utf8_easy((char *)messages_get("SaveIFF"));

	ctxmenulab[CMID_SAVEURL] = ami_utf8_easy((char *)messages_get("LinkDload"));
	ctxmenulab[CMID_URLOPENWIN] = ami_utf8_easy((char *)messages_get("LinkNewWin"));
	ctxmenulab[CMID_URLOPENTAB] = ami_utf8_easy((char *)messages_get("LinkNewTab"));

	ctxmenulab[CMID_SELCUT] = ami_utf8_easy((char *)messages_get("CutNS"));
	ctxmenulab[CMID_SELCOPY] = ami_utf8_easy((char *)messages_get("CopyNS"));
	ctxmenulab[CMID_SELPASTE] = ami_utf8_easy((char *)messages_get("PasteNS"));
	ctxmenulab[CMID_SELALL] = ami_utf8_easy((char *)messages_get("SelectAllNS"));
	ctxmenulab[CMID_SELCLEAR] = ami_utf8_easy((char *)messages_get("ClearNS"));
	ctxmenulab[CMID_SELSEARCH] = ami_utf8_easy((char *)messages_get("SearchWeb"));

	ctxmenulab[CMID_PLUGINCMD] = ami_utf8_easy((char *)messages_get("ExternalApp"));

	ctxmenulab[CMSUB_OBJECT] = ami_utf8_easy((char *)messages_get("Object"));
	ctxmenulab[CMSUB_URL] = ami_utf8_easy((char *)messages_get("Link"));
	ctxmenulab[CMSUB_SEL] = ami_utf8_easy((char *)messages_get("Selection"));

	/* Back button */
	ctxmenulab[CMID_HISTORY] = ami_utf8_easy((char *)messages_get("HistLocalNS"));
}

void ami_context_menu_free(void)
{
	int i;

	for(i=0;i<CMID_LAST;i++)
	{
		ami_utf8_free(ctxmenulab[i]);
	}

	if(IPopupMenu) DropInterface((struct Interface *)IPopupMenu);
	if(PopupMenuBase) CloseLibrary(PopupMenuBase);
}

BOOL ami_context_menu_mouse_trap(struct gui_window_2 *gwin, BOOL trap)
{
	int top, left, width, height;

	if(option_context_menu == false) return FALSE;

	if((option_kiosk_mode == false) && (trap == FALSE) &&
		(gwin->bw->browser_window_type == BROWSER_WINDOW_NORMAL))
	{
		if(browser_window_back_available(gwin->bw) &&
				ami_gadget_hit(gwin->objects[GID_BACK],
				gwin->win->MouseX, gwin->win->MouseY))
			trap = TRUE;

		if(browser_window_forward_available(gwin->bw) &&
				ami_gadget_hit(gwin->objects[GID_FORWARD],
				gwin->win->MouseX, gwin->win->MouseY))
			trap = TRUE;
	}

	if(gwin->rmbtrapped == trap) return trap;

	SetWindowAttr(gwin->win, WA_RMBTrap, trap, 1);
	gwin->rmbtrapped = trap;

	return trap;
}

void ami_context_menu_show(struct gui_window_2 *gwin,int x,int y)
{
	struct hlcache_handle *cc = gwin->bw->current_content;
	struct box *curbox;
	int box_x=0;
	int box_y=0;
	bool menuhascontent = false;
	bool no_url = true, no_obj = true, no_sel = true;

	if(!cc) return;
	if(content_get_type(cc) != CONTENT_HTML) return;

	if(gwin->objects[OID_MENU]) DisposeObject(gwin->objects[OID_MENU]);

	gwin->popuphook.h_Entry = ami_context_menu_hook;
	gwin->popuphook.h_Data = gwin;

    gwin->objects[OID_MENU] = NewObject( POPUPMENU_GetClass(), NULL,
                        PMA_MenuHandler, &gwin->popuphook,
						TAG_DONE);

	if(gwin->bw && gwin->bw->history &&
		ami_gadget_hit(gwin->objects[GID_BACK],
			gwin->win->MouseX, gwin->win->MouseY))
	{
		gwin->temp = 0;
		history_enumerate_back(gwin->bw->history, ami_context_menu_history, gwin);

		IDoMethod(gwin->objects[OID_MENU], PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, ~0,
			TAG_DONE),
		~0);

		IDoMethod(gwin->objects[OID_MENU], PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, (ULONG)ctxmenulab[CMID_HISTORY],
				PMIA_ID, CMID_HISTORY,
				PMIA_UserData, NULL,
			TAG_DONE),
		~0);

		menuhascontent = true;
	}
	else if(gwin->bw && gwin->bw->history &&
		ami_gadget_hit(gwin->objects[GID_FORWARD],
			gwin->win->MouseX, gwin->win->MouseY))
	{
		gwin->temp = 0;
		history_enumerate_forward(gwin->bw->history, ami_context_menu_history, gwin);

		IDoMethod(gwin->objects[OID_MENU], PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, ~0,
			TAG_DONE),
		~0);

		IDoMethod(gwin->objects[OID_MENU], PM_INSERT,
			NewObject(POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, (ULONG)ctxmenulab[CMID_HISTORY],
				PMIA_ID, CMID_HISTORY,
				PMIA_UserData, NULL,
			TAG_DONE),
		~0);

		menuhascontent = true;
	}
	else
	{
		curbox = html_get_box_tree(gwin->bw->current_content);

		while(curbox = box_at_point(curbox,x,y,&box_x,&box_y,&cc))
		{
			if (curbox->style &&
				css_computed_visibility(curbox->style) == CSS_VISIBILITY_HIDDEN)
			continue;

			if(no_url && curbox->href)
			{
				IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
					NewObject(POPUPMENU_GetItemClass(), NULL,
						PMIA_Title, (ULONG)ctxmenulab[CMSUB_URL],
						PMSIMPLESUB,
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_URLOPENWIN],
								PMIA_ID,CMID_URLOPENWIN,
								PMIA_UserData,curbox->href,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_URLOPENTAB],
								PMIA_ID,CMID_URLOPENTAB,
								PMIA_UserData,curbox->href,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_COPYURL],
								PMIA_ID,CMID_COPYURL,
								PMIA_UserData,curbox->href,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SAVEURL],
								PMIA_ID,CMID_SAVEURL,
								PMIA_UserData,curbox->href,
							TAG_DONE),
						TAG_DONE),
					TAG_DONE),
				~0);

				no_url = false;
				menuhascontent = true;
			}

			if(no_obj && curbox->object &&
				(content_get_type(curbox->object) == CONTENT_IMAGE))
			{
				IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
					NewObject(POPUPMENU_GetItemClass(), NULL,
						PMIA_Title, (ULONG)ctxmenulab[CMSUB_OBJECT],
						PMSIMPLESUB,
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SHOWOBJ],
								PMIA_ID,CMID_SHOWOBJ,
								PMIA_UserData, content_get_url(curbox->object),
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_COPYOBJ],
								PMIA_ID,CMID_COPYOBJ,
								PMIA_UserData, content_get_url(curbox->object),
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_CLIPOBJ],
								PMIA_ID,CMID_CLIPOBJ,
								PMIA_UserData,curbox->object,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SAVEOBJ],
								PMIA_ID,CMID_SAVEOBJ,
								PMIA_UserData,curbox->object,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SAVEIFFOBJ],
								PMIA_ID,CMID_SAVEIFFOBJ,
								PMIA_UserData,curbox->object,
							TAG_DONE),
						TAG_DONE),
					TAG_DONE),
				~0);

				no_obj = false;
				menuhascontent = true;
			}

			if(no_sel && (curbox->text) ||
				(curbox->gadget && ((curbox->gadget->type == GADGET_TEXTBOX) ||
				(curbox->gadget->type == GADGET_TEXTAREA) ||
				(curbox->gadget->type == GADGET_PASSWORD))))
			{
				BOOL disabled_readonly = selection_read_only(gwin->bw->sel);
				BOOL disabled_noselection = !selection_defined(gwin->bw->sel);

				IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
					NewObject(POPUPMENU_GetItemClass(), NULL,
						PMIA_Title, (ULONG)ctxmenulab[CMSUB_SEL],
						PMIA_SubMenu, NewObject(POPUPMENU_GetClass(), NULL,
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SELCUT],
								PMIA_ID,CMID_SELCUT,
								PMIA_Disabled, disabled_noselection && disabled_readonly,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SELCOPY],
								PMIA_ID,CMID_SELCOPY,
								PMIA_Disabled, disabled_noselection,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SELPASTE],
								PMIA_ID,CMID_SELPASTE,
								PMIA_Disabled, (gwin->bw->window->c_h == 0),
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SELALL],
								PMIA_ID,CMID_SELALL,
								//PMIA_UserData,curbox->href,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SELCLEAR],
								PMIA_ID,CMID_SELCLEAR,
								PMIA_Disabled, disabled_noselection,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, ~0,
							TAG_DONE),
							PMA_AddItem,NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SELSEARCH],
								PMIA_ID,CMID_SELSEARCH,
								PMIA_Disabled, disabled_noselection,
							TAG_DONE),
						TAG_DONE),
					TAG_DONE),
				~0);

				no_sel = false;
				menuhascontent = true;
			}

			if(curbox->object &&
				(content_get_type(curbox->object) == CONTENT_PLUGIN))
			{
				if(ami_mime_content_to_cmd(curbox->object))
				{
					IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
						NewObject(POPUPMENU_GetItemClass(), NULL,
							PMIA_Title, (ULONG)ctxmenulab[CMID_PLUGINCMD],
							PMIA_ID, CMID_PLUGINCMD,
							PMIA_UserData, curbox->object,
							TAG_DONE),
						~0);

					menuhascontent = true;
				}
			}
			if (curbox->gadget)
			{
				switch (curbox->gadget->type)
				{
					case GADGET_FILE:
						IDoMethod(gwin->objects[OID_MENU],PM_INSERT,
							NewObject(POPUPMENU_GetItemClass(), NULL,
								PMIA_Title, (ULONG)ctxmenulab[CMID_SELECTFILE],
								PMIA_ID,CMID_SELECTFILE,
								PMIA_UserData,curbox,
								TAG_DONE),
							~0);

						menuhascontent = true;
					break;
				}
			}
		}
	}

	if(!menuhascontent) return;

	gui_window_set_pointer(gwin->bw->window,GUI_POINTER_DEFAULT);
	IDoMethod(gwin->objects[OID_MENU],PM_OPEN,gwin->win);
}

static uint32 ami_context_menu_hook(struct Hook *hook,Object *item,APTR reserved)
{
    int32 itemid = 0;
	struct gui_window_2 *gwin = hook->h_Data;
	APTR userdata = NULL;
	struct browser_window *bw;
	struct hlcache_handle *object;
	char *source_data;
	ULONG source_size;
	struct bitmap *bm;

    if(GetAttrs(item,PMIA_ID,&itemid,
					PMIA_UserData,&userdata,
					TAG_DONE))
    {
		switch(itemid)
		{
			case CMID_SELECTFILE:
				if(AslRequestTags(filereq,
					ASLFR_TitleText,messages_get("NetSurf"),
					ASLFR_Screen,scrn,
					ASLFR_DoSaveMode,FALSE,
					TAG_DONE))
				{
					struct box *box = userdata;
					char *utf8_fn;
					char fname[1024];
					int x,y;

					strlcpy(&fname,filereq->fr_Drawer,1024);
					AddPart(fname,filereq->fr_File,1024);

					if(utf8_from_local_encoding(fname,0,&utf8_fn) != UTF8_CONVERT_OK)
					{
						warn_user("NoMemory","");
						break;
					}

					free(box->gadget->value);
					box->gadget->value = utf8_fn;

					box_coords(box, (int *)&x, (int *)&y);
					ami_do_redraw_limits(gwin->bw->window, 
						gwin->bw->window->shared->bw,
						x,y,
						x + box->width,
						y + box->height);
				}
			break;

			case CMID_COPYURL:
			case CMID_COPYOBJ:
				ami_easy_clipboard((char *)userdata);
			break;

			case CMID_URLOPENWIN:
				bw = browser_window_create(userdata, gwin->bw,
					content_get_url(gwin->bw->current_content), true, false);
			break;

			case CMID_URLOPENTAB:
				bw = browser_window_create(userdata, gwin->bw,
					content_get_url(gwin->bw->current_content), true, true);
			break;

			case CMID_SAVEURL:
				browser_window_download(gwin->bw, userdata,
					content_get_url(gwin->bw->current_content));
			break;

			case CMID_SHOWOBJ:
				browser_window_go(gwin->bw, userdata,
					content_get_url(gwin->bw->current_content), true);
			break;

			case CMID_CLIPOBJ:
				object = (struct hlcache_handle *)userdata;
				if((bm = content_get_bitmap(object)))
				{
					bm->url = content_get_url(object);
					bm->title = content_get_title(object);
					ami_easy_clipboard_bitmap(bm);
				}
#ifdef WITH_NS_SVG
				else if(ami_mime_compare(object, "svg") == true)
				{
					ami_easy_clipboard_svg(object);
				}
#endif
			break;

			case CMID_SAVEOBJ:
				object = (struct hlcache_handle *)userdata;

				if(AslRequestTags(savereq,
							ASLFR_TitleText,messages_get("NetSurf"),
							ASLFR_Screen,scrn,
							ASLFR_InitialFile,FilePart(content_get_url(object)),
							TAG_DONE))
				{
					BPTR fh = 0;
					char fname[1024];
					strlcpy(&fname,savereq->fr_Drawer,1024);
					AddPart(fname,savereq->fr_File,1024);
					ami_update_pointer(gwin->win,GUI_POINTER_WAIT);

					if(ami_download_check_overwrite(fname, gwin->win))
					{
						if(fh = FOpen(fname,MODE_NEWFILE,0))
						{
							if((source_data =
								content_get_source_data(object, &source_size)))
									FWrite(fh, source_data, 1, source_size);

							FClose(fh);
							SetComment(fname, content_get_url(object));
						}
					}
					ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
				}
			break;

			case CMID_SAVEIFFOBJ:
				object = (struct hlcache_handle *)userdata;

				if(AslRequestTags(savereq,
							ASLFR_TitleText,messages_get("NetSurf"),
							ASLFR_Screen,scrn,
							ASLFR_InitialFile,FilePart(content_get_url(object)),
							TAG_DONE))
				{
					BPTR fh = 0;
					char fname[1024];

					strlcpy(&fname,savereq->fr_Drawer,1024);
					AddPart(fname,savereq->fr_File,1024);
					if((bm = content_get_bitmap(object)))
					{
						bm->url = content_get_url(object);
						bm->title = content_get_title(object);
						if(bitmap_save(bm, fname, 0))
							SetComment(fname, content_get_url(object));
					}
#ifdef WITH_NS_SVG
					else if(ami_mime_compare(object, "svg") == true)
					{
						if(ami_save_svg(object,fname))
							SetComment(fname, content_get_url(object));
					}
#endif
					ami_update_pointer(gwin->win,GUI_POINTER_DEFAULT);
				}
			break;

			case CMID_PLUGINCMD:
				amiga_plugin_hack_execute((struct hlcache_handle *)userdata);
			break;

			case CMID_HISTORY:
				if(userdata == NULL)
				{
					ami_history_open(gwin->bw, gwin->bw->history);
				}
				else
				{
					history_go(gwin->bw, gwin->bw->history,
						(struct history_entry *)userdata, false);
				}
			break;

			case CMID_SELCUT:
				browser_window_key_press(gwin->bw, KEY_CUT_SELECTION);
			break;

			case CMID_SELCOPY:
				browser_window_key_press(gwin->bw, KEY_COPY_SELECTION);
				browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
			break;

			case CMID_SELPASTE:
				browser_window_key_press(gwin->bw, KEY_PASTE);
			break;

			case CMID_SELALL:
				browser_window_key_press(gwin->bw, KEY_SELECT_ALL);
				gui_start_selection(gwin->bw->window);
			break;

			case CMID_SELCLEAR:
				browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
			break;

			case CMID_SELSEARCH:
			{
				struct ami_text_selection *sel;
				char *url;

				if(sel = ami_selection_to_text(gwin))
				{
					url = search_web_from_term(sel->text);
					browser_window_go(gwin->bw, url, NULL, true);

					FreeVec(sel);
				}
			}
			break;
		}
    }

    return itemid;
}

static bool ami_context_menu_history(const struct history *history, int x0, int y0,
	int x1, int y1, const struct history_entry *entry, void *user_data)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)user_data;

	gwin->temp++;
	if(gwin->temp > 10) return false;

	IDoMethod(gwin->objects[OID_MENU], PM_INSERT,
		NewObject(POPUPMENU_GetItemClass(), NULL,
			PMIA_Title, (ULONG)history_entry_get_title(entry),
			PMIA_ID, CMID_HISTORY,
			PMIA_UserData, entry,
		TAG_DONE),
	~0);

	return true;
}

uint32 ami_popup_hook(struct Hook *hook,Object *item,APTR reserved)
{
	int32 itemid = 0;
	struct gui_window *gwin = hook->h_Data;

	if(GetAttr(PMIA_ID, item, &itemid))
	{
		form_select_process_selection(gwin->shared->bw->current_content,gwin->shared->control,itemid);
	}

	return itemid;
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
	/* TODO: PMIA_Title memory leaks as we don't free the strings.
	 * We use the core menu anyway, but in future when popupmenu.class
	 * improves we will probably start using this again.
	 */

	struct gui_window *gwin = bw->window;
	struct form_option *opt = control->data.select.items;
	ULONG i = 0;

	if(gwin->shared->objects[OID_MENU]) DisposeObject(gwin->shared->objects[OID_MENU]);

	gwin->shared->popuphook.h_Entry = ami_popup_hook;
	gwin->shared->popuphook.h_Data = gwin;

	gwin->shared->control = control;

    gwin->shared->objects[OID_MENU] = PMMENU(ami_utf8_easy(control->name)),
                        PMA_MenuHandler, &gwin->shared->popuphook, End;

	while(opt)
	{
		IDoMethod(gwin->shared->objects[OID_MENU], PM_INSERT,
			NewObject( POPUPMENU_GetItemClass(), NULL,
				PMIA_Title, (ULONG)ami_utf8_easy(opt->text),
				PMIA_ID, i,
				PMIA_CheckIt, TRUE,
				PMIA_Checked, opt->selected,
				TAG_DONE),
			~0);

		opt = opt->next;
		i++;
	}

	gui_window_set_pointer(gwin, GUI_POINTER_DEFAULT); // Clear the menu-style pointer

	IDoMethod(gwin->shared->objects[OID_MENU], PM_OPEN, gwin->shared->win);
}

#else

void ami_context_menu_init(void)
{
}

void ami_context_menu_free(void)
{
}

BOOL ami_context_menu_mouse_trap(struct gui_window_2 *gwin, BOOL trap)
{
	return FALSE;
}

void ami_context_menu_show(struct gui_window_2 *gwin, int x, int y)
{
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
}
#endif
