#ifndef GEMTK_H_INCLUDED
#define GEMTK_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

#include <mint/osbind.h>
#include <mint/cookie.h>

#include <gem.h>
#include <cflib.h>


/* -------------------------------------------------------------------------- */
/* SYSTEM UTILS                                                               */
/* -------------------------------------------------------------------------- */

/* System type detection added by [GS]  */
/* detect the system type, AES + kernel */
#define SYS_TOS    0x0001
#define SYS_MAGIC  0x0002
#define SYS_MINT   0x0004
#define SYS_GENEVA 0x0010
#define SYS_NAES   0x0020
#define SYS_XAAES  0x0040
#define sys_type()    (_systype_v ? _systype_v : _systype())
#define sys_MAGIC()   ((sys_type() & SYS_MAGIC) != 0)
#define sys_NAES()    ((sys_type() & SYS_NAES)  != 0)
#define sys_XAAES()   ((sys_type() & SYS_XAAES) != 0)

#define TOS4VER 0x03300 /* this is assumed to be the last single tasking OS */

extern unsigned short _systype_v;
unsigned short _systype (void);

/*
* Chech for GRECT intersection without modifiend the src rectangles
* return true when the  GRECT's intersect, fals otherwise.
*/
bool rc_intersect_ro(GRECT *a, GRECT *b);

/*
* Convert keycode returned by evnt_multi to ascii value
*/
int keybd2ascii( int keybd, int shift);

/** set VDI clip area by passing an GRECT */
void gemtk_clip_grect(VdiHdl vh, GRECT *rect);

#ifndef POINT_WITHIN
# define POINT_WITHIN(_x,_y, r) ((_x >= r.g_x) && (_x <= r.g_x + r.g_w ) \
		&& (_y >= r.g_y) && (_y <= r.g_y + r.g_h))
#endif

#ifndef RC_WITHIN
# define RC_WITHIN(a,b) \
    (((a)->g_x >= (b)->g_x) \
        && (((a)->g_x + (a)->g_w) <= ((b)->g_x + (b)->g_w))) \
            && (((a)->g_y >= (b)->g_y) \
                && (((a)->g_y + (a)->g_h) <= ((b)->g_y + (b)->g_h)))
#endif

#ifndef MAX
# define MAX(_a,_b) ((_a>_b) ? _a : _b)
#endif

#ifndef MIN
# define MIN(_a,_b) ((_a<_b) ? _a : _b)
#endif

#ifndef SET_BIT
# define SET_BIT(field,bit,val) field = (val)?((field)|(bit)):((field) & ~(bit))
#endif

/* -------------------------------------------------------------------------- */
/* MultiTOS Drag & Drop                                                       */
/* -------------------------------------------------------------------------- */
short ddcreate(short *pipe);
short ddmessage(short apid, short fd, short winid, short mx, short my, short kstate, short pipename);
short ddrexts(short fd, char *exts);
short ddstry(short fd, char *ext, char *text, char *name, long size);
void ddclose(short fd);
void ddgetsig(long *oldsig);
void ddsetsig(long oldsig);
short ddopen(short ddnam, char ddmsg);
short ddsexts(short fd, char *exts);
short ddrtry(short fd, char *name, char *file, char *whichext, long *size);
short ddreply(short fd, char ack);

/* -------------------------------------------------------------------------- */
/* Message Box module                                                         */
/* -------------------------------------------------------------------------- */
#define MSG_BOX_ALERT	1
#define MSG_BOX_CONFIRM	2

short msg_box_show(short type, const char * msg);

/* -------------------------------------------------------------------------- */
/* GUIWIN Module                                                              */
/* -------------------------------------------------------------------------- */
#define GW_FLAG_PREPROC_WM			0x01	// let guiwin API handle some events
#define GW_FLAG_RECV_PREPROC_WM		0x02	// get notified even when pre-processed
#define GW_FLAG_HAS_VTOOLBAR		0x04	// the attached toolbar is vertical
#define GW_FLAG_CUSTOM_TOOLBAR		0x08	// no internal toolbar handling
#define GW_FLAG_CUSTOM_SCROLLING	0x20	// no internal scroller handling

#define GW_FLAG_DEFAULTS (GW_FLAG_PREPROC_WM | GW_FLAG_RECV_PREPROC_WM)

#define GW_STATUS_ICONIFIED			0x01
#define GW_STATUS_SHADED			0x02

#define GW_CB_SELECTED 				(OS_SELECTED)

#define GUIWIN_VSLIDER 				0x01
#define GUIWIN_HSLIDER 				0x02
#define GUIWIN_VH_SLIDER 			0x03

/*
	Message sent to the client application when an AES object is
	clicked in an window which contains an form.

	Message Parameters:
	msg[4] = Clicked Object.
	msg[5] = Number of clicks.
	msg[6] = Modifier keys.
*/
#define GUIWIN_WM_FORM				1001

struct gui_window_s;

/** list struct for managing AES windows */
typedef struct gui_window_s GUIWIN;

/** GUIWIN event handler */
typedef short (*guiwin_event_handler_f)(GUIWIN *gw,
		EVMULT_OUT *ev_out, short msg[8]);

typedef void (*guiwin_redraw_f)(GUIWIN *win, uint16_t msg, GRECT *clip);

struct guiwin_scroll_info_s {

	/** Definition of a content unit (horizontal) measured in pixel  */
	int x_unit_px;

	/** Definition of content unit (vertical) measured in pixel */
	int y_unit_px;

	/** Current scroll position (in content units) */
	int x_pos;

	/** Current scroll position (in content units) */
	int y_pos;

	/** Size of content (horizontal) measured in content units */
	int x_units;

	/** Size of content (vertical) measured in content units */
	int y_units;
};

/** Well known areas inside the window */
enum guwin_area_e {
	GUIWIN_AREA_WORK = 0,
	GUIWIN_AREA_TOOLBAR,
	GUIWIN_AREA_CONTENT
};

/* -------------------------------------------------------------------------- */
/* GUIWIN functions (document in guiwin.c)                                    */
/* -------------------------------------------------------------------------- */

short
guiwin_init(void);

void
guiwin_exit(void);

GUIWIN * guiwin_add(short handle, uint32_t flags,
		guiwin_event_handler_f handler);

GUIWIN * guiwin_find(short handle);

short guiwin_remove(GUIWIN *win);

GUIWIN * guiwin_validate_ptr(GUIWIN *win);

short guiwin_dispatch_event(EVMULT_IN *ev_in, EVMULT_OUT *ev_out, short msg[8]);

void guiwin_get_grect(GUIWIN *win, enum guwin_area_e mode, GRECT *dest);

short guiwin_get_handle(GUIWIN *win);

uint32_t guiwin_get_state(GUIWIN *win);

void guiwin_set_toolbar(GUIWIN *win, OBJECT *toolbar, short idx,
		uint32_t flags);

void guiwin_set_event_handler(GUIWIN *win,guiwin_event_handler_f cb);

void guiwin_set_user_data(GUIWIN *win, void *data);

void * guiwin_get_user_data(GUIWIN *win);

struct guiwin_scroll_info_s * guiwin_get_scroll_info(GUIWIN *win);

void guiwin_set_scroll_grid(GUIWIN * win, short x, short y);

void guiwin_set_content_units(GUIWIN * win, short x, short y);

void guiwin_set_form(GUIWIN *win, OBJECT *tree, short index);

void guiwin_set_toolbar_size(GUIWIN *win, uint16_t s);

void guiwin_set_toolbar_redraw_func(GUIWIN *win, guiwin_redraw_f func);

bool guiwin_update_slider(GUIWIN *win, short mode);

void guiwin_scroll(GUIWIN *gw, short orientation, int units, bool refresh);

void guiwin_send_msg(GUIWIN *win, short msgtype, short a, short b, short c,
		short d);

void guiwin_send_redraw(GUIWIN *win, GRECT *area);

VdiHdl guiwin_get_vdi_handle(GUIWIN *win);

bool guiwin_has_intersection(GUIWIN *win, GRECT *work);

void guiwin_toolbar_redraw(GUIWIN *win, uint16_t msg, GRECT *clip);

void guiwin_form_redraw(GUIWIN *gw, GRECT *clip);

void guiwin_clear(GUIWIN *win);

/* -------------------------------------------------------------------------- */
/* AES SCROLLER MODULE                                                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* AES TABS MODULE                                                            */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* AES OBJECT TREE TOOLS                                                      */
/* -------------------------------------------------------------------------- */
char gemtk_obj_set_str_safe(OBJECT * tree, short idx, char *txt);
char *get_text(OBJECT * tree, short idx);
GRECT * obj_screen_rect(OBJECT * tree, short obj);
bool obj_is_inside(OBJECT * tree, short obj, GRECT *area);
OBJECT *get_tree(int idx);
void gemtk_obj_mouse_sprite(OBJECT *tree, int index);
#endif // GEMTK_H_INCLUDED
