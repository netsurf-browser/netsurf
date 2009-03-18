/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <signal.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>

#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/fb.h>

#include <linux/input.h>

#include "css/css.h"
#include "utils/messages.h"
#include "desktop/gui.h"
/* #include "desktop/textinput.h" cannot include this because it conflicts with
    the linux defines */
#define NSKEY_PAGE_DOWN 135
#define NSKEY_PAGE_UP 134
#define NSKEY_DOWN 31
#define NSKEY_UP 30
#define NSKEY_LEFT 28
#define NSKEY_RIGHT 29
#define NSKEY_ESCAPE 27

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_tk.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_schedule.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_options.h"

#include "utils/log.h"
#include "utils/messages.h"

#define FB_ACTIVE    0
#define FB_REL_REQ   1
#define FB_INACTIVE  2
#define FB_ACQ_REQ   3


struct fb_fix_screeninfo fb_fix;
struct fb_var_screeninfo fb_var;
unsigned char *fb_mem;
int fb_mem_offset = 0;
int fb_switch_state = FB_ACTIVE;

static int fb,tty;
static int orig_vt_no = 0;
static struct vt_mode vt_mode;

static int kd_mode;
static struct vt_mode vt_omode;
static struct termios term;
static struct fb_var_screeninfo fb_ovar;
static unsigned short ored[256], ogreen[256], oblue[256], otransp[256];
static struct fb_cmap ocmap = { 0, 256, ored, ogreen, oblue, otransp };

typedef struct _input_dev {
        struct _input_dev *next;
        int fd;
} fb_input_dev;

static fb_input_dev *inputdevs = 0;

/* -------------------------------------------------------------------- */
/* devices                                                              */

struct DEVS {
        const char *fb0;
        const char *fbnr;
        const char *ttynr;
};

struct DEVS devs_default = {
        .fb0 =  "/dev/fb0",
        .fbnr = "/dev/fb%d",
        .ttynr = "/dev/tty%d",
};

struct DEVS *devices;


static char *
fconcat(const char *base, const char *leaf)
{
        static char buffer[PATH_MAX];
        snprintf(buffer, PATH_MAX, "%s/%s", base, leaf);
        return buffer;
}

/* Input device opening */
static void
fb_open_input_devices(void)
{
        DIR *dir;
        fb_input_dev *d;
        struct dirent *de;
        const char *basepath = option_fb_input_devpath ? option_fb_input_devpath : "/dev/input";

        dir = opendir(basepath);

        if (dir == NULL)
                return;

        while ((de = readdir(dir)) != NULL) {
                if (fnmatch(option_fb_input_glob ?
                            option_fb_input_glob : "event*",
                            de->d_name, 0) == 0) {
                        char *cc = fconcat(basepath, de->d_name);
                        int fd = open(cc, O_RDONLY | O_NONBLOCK);
                        if (fd >= 0) {
                                d = calloc(1, sizeof(fb_input_dev));
                                d->next = inputdevs;
                                inputdevs = d;
                                d->fd = fd;
                        }
                }
        }

        closedir(dir);
}

/* -------------------------------------------------------------------- */
/* console switching                                                    */


static void
fb_switch_signal(int signal)
{
        if (signal == SIGUSR1) {
                /* release */
                fb_switch_state = FB_REL_REQ;
        }
        if (signal == SIGUSR2) {
                /* acquisition */
                fb_switch_state = FB_ACQ_REQ;
        }
}

static int
fb_switch_init(void)
{
        struct sigaction act,old;

        memset(&act, 0, sizeof(act));
        act.sa_handler = fb_switch_signal;
        sigemptyset(&act.sa_mask);
        sigaction(SIGUSR1, &act, &old);
        sigaction(SIGUSR2, &act, &old);

        if (ioctl(tty,VT_GETMODE, &vt_mode) == -1) {
                perror("ioctl VT_GETMODE");
                exit(1);
        }
        vt_mode.mode   = VT_PROCESS;
        vt_mode.waitv  = 0;
        vt_mode.relsig = SIGUSR1;
        vt_mode.acqsig = SIGUSR2;

        if (ioctl(tty,VT_SETMODE, &vt_mode) == -1) {
                perror("ioctl VT_SETMODE");
                exit(1);
        }
        return 0;
}

/* -------------------------------------------------------------------- */
/* initialisation & cleanup                                             */

static int
fb_setmode(const char *name, int bpp)
{
        FILE *fp;
        char line[80], label[32], value[16];
        int geometry = 0;
        int timings = 0;

        /* load current values */
        if (ioctl(fb, FBIOGET_VSCREENINFO, &fb_var) == -1) {
                perror("ioctl FBIOGET_VSCREENINFO");
                exit(1);
        }

        if (name == NULL)
                return -1;

        if ((fp = fopen("/etc/fb.modes","r")) == NULL)
                return -1;

        while (NULL != fgets(line,79,fp)) {
                if ((sscanf(line, "mode \"%31[^\"]\"", label) == 1) &&
                    (strcmp(label,name) == 0)) {
                        /* fill in new values */
                        fb_var.sync  = 0;
                        fb_var.vmode = 0;
                        while ((fgets(line,79,fp) != NULL) &&
                               (strstr(line,"endmode") == NULL)) {
                                if (5 == sscanf(line," geometry %d %d %d %d %d",
                                                &fb_var.xres,&fb_var.yres,
                                                &fb_var.xres_virtual,&fb_var.yres_virtual,
                                                &fb_var.bits_per_pixel))
                                        geometry = 1;

                                if (7 == sscanf(line," timings %d %d %d %d %d %d %d",
                                                &fb_var.pixclock,
                                                &fb_var.left_margin,  &fb_var.right_margin,
                                                &fb_var.upper_margin, &fb_var.lower_margin,
                                                &fb_var.hsync_len,    &fb_var.vsync_len))
                                        timings = 1;

                                if (1 == sscanf(line, " hsync %15s",value) &&
                                    0 == strcasecmp(value,"high"))
                                        fb_var.sync |= FB_SYNC_HOR_HIGH_ACT;

                                if (1 == sscanf(line, " vsync %15s",value) &&
                                    0 == strcasecmp(value,"high"))
                                        fb_var.sync |= FB_SYNC_VERT_HIGH_ACT;

                                if (1 == sscanf(line, " csync %15s",value) &&
                                    0 == strcasecmp(value,"high"))
                                        fb_var.sync |= FB_SYNC_COMP_HIGH_ACT;

                                if (1 == sscanf(line, " extsync %15s",value) &&
                                    0 == strcasecmp(value,"true"))
                                        fb_var.sync |= FB_SYNC_EXT;

                                if (1 == sscanf(line, " laced %15s",value) &&
                                    0 == strcasecmp(value,"true"))
                                        fb_var.vmode |= FB_VMODE_INTERLACED;

                                if (1 == sscanf(line, " double %15s",value) &&
                                    0 == strcasecmp(value,"true"))
                                        fb_var.vmode |= FB_VMODE_DOUBLE;
                        }
                        /* ok ? */
                        if (!geometry || !timings)
                                return -1;

                        if (bpp != 0)
                                fb_var.bits_per_pixel = bpp;

                        /* set */
                        fb_var.xoffset = 0;
                        fb_var.yoffset = 0;
                        if (ioctl(fb, FBIOPUT_VSCREENINFO, &fb_var) == -1)
                                perror("ioctl FBIOPUT_VSCREENINFO");

                        /* look what we have now ... */
                        if (ioctl(fb, FBIOGET_VSCREENINFO, &fb_var) == -1) {
                                perror("ioctl FBIOGET_VSCREENINFO");
                                exit(1);
                        }
                        return 0;
                }
        }
        return -1;
}

static void
fb_setvt(int vtno)
{
        struct vt_stat vts;
        char vtname[12];

        if (vtno < 0) {
                if ((ioctl(tty, VT_OPENQRY, &vtno) == -1) || (vtno == -1)) {
                        perror("ioctl VT_OPENQRY");
                        exit(1);
                }
        }

        vtno &= 0xff;
        sprintf(vtname, devices->ttynr, vtno);
        chown(vtname, getuid(), getgid());
        if (-1 == access(vtname, R_OK | W_OK)) {
                fprintf(stderr,"access %s: %s\n",vtname,strerror(errno));
                exit(1);
        }

        tty = open(vtname,O_RDWR);
        if (ioctl(tty,VT_GETSTATE, &vts) == -1) {
                perror("ioctl VT_GETSTATE");
                exit(1);
        }

        orig_vt_no = vts.v_active;
        if (ioctl(tty,VT_ACTIVATE, vtno) == -1) {
                perror("ioctl VT_ACTIVATE");
                exit(1);
        }
        if (ioctl(tty,VT_WAITACTIVE, vtno) == -1) {
                perror("ioctl VT_WAITACTIVE");
                exit(1);
        }
}

/* Hmm. radeonfb needs this. matroxfb doesn't. */
static int
fb_activate_current(int tty)
{
        struct vt_stat vts;

        if (ioctl(tty,VT_GETSTATE, &vts) == -1) {
                perror("ioctl VT_GETSTATE");
                return -1;
        }
        if (ioctl(tty,VT_ACTIVATE, vts.v_active) == -1) {
                perror("ioctl VT_ACTIVATE");
                return -1;
        }
        if (ioctl(tty,VT_WAITACTIVE, vts.v_active) == -1) {
                perror("ioctl VT_WAITACTIVE");
                return -1;
        }
        return 0;
}

static void
fb_cleanup(void)
{
        /* restore console */
        if (ioctl(fb,FBIOPUT_VSCREENINFO,&fb_ovar) == -1)
                perror("ioctl FBIOPUT_VSCREENINFO");

        if (ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix) == -1)
                perror("ioctl FBIOGET_FSCREENINFO");

        if ((fb_ovar.bits_per_pixel == 8) ||
            (fb_fix.visual == FB_VISUAL_DIRECTCOLOR)) {
                if (ioctl(fb,FBIOPUTCMAP,&ocmap) == -1)
                        perror("ioctl FBIOPUTCMAP");
        }
        close(fb);

        if (ioctl(tty,KDSETMODE, kd_mode) == -1)
                perror("ioctl KDSETMODE");

        if (ioctl(tty,VT_SETMODE, &vt_omode) == -1)
                perror("ioctl VT_SETMODE");

        if (orig_vt_no && (ioctl(tty, VT_ACTIVATE, orig_vt_no) == -1))
                perror("ioctl VT_ACTIVATE");

        if (orig_vt_no && (ioctl(tty, VT_WAITACTIVE, orig_vt_no) == -1))
                perror("ioctl VT_WAITACTIVE");

        tcsetattr(tty, TCSANOW, &term);
        close(tty);
}

static int
framebuffer_init(const char *device, int width, int height, int refresh, int bpp, int vt)
{
        char fbdev[16];
        struct vt_stat vts;
        long pm = ~(sysconf(_SC_PAGESIZE) - 1);
        char mode[32];

        snprintf(mode, 32, "%dx%d-%d", width, height, refresh);

        devices = &devs_default;

        tty = 0;
        if (vt != 0)
                fb_setvt(vt);

        if (ioctl(tty,VT_GETSTATE, &vts) == -1) {
                fprintf(stderr,
                        "ioctl VT_GETSTATE: %s (not a linux console?)\n",
                        strerror(errno));
                exit(1);
        }

        if (device == NULL) {
                device = getenv("FRAMEBUFFER");
                if (device == NULL) {
                        struct fb_con2fbmap c2m;
                        if ((fb = open(devices->fb0, O_RDWR, 0)) == -1) {
                                fprintf(stderr,
                                        "open %s: %s\n",
                                        devices->fb0,
                                        strerror(errno));
                                exit(1);
                        }
                        c2m.console = vts.v_active;
                        if (ioctl(fb, FBIOGET_CON2FBMAP, &c2m) == -1) {
                                perror("ioctl FBIOGET_CON2FBMAP");
                                exit(1);
                        }
                        close(fb);
                        fprintf(stderr,"map: vt%02d => fb%d\n",
                                c2m.console, c2m.framebuffer);
                        sprintf(fbdev, devices->fbnr, c2m.framebuffer);
                        device = fbdev;
                }
        }

        /* get current settings (which we have to restore) */
        if ((fb = open(device, O_RDWR)) == -1) {
                fprintf(stderr, "open %s: %s\n", device, strerror(errno));
                exit(1);
        }

        if (ioctl(fb, FBIOGET_VSCREENINFO, &fb_ovar) == -1) {
                perror("ioctl FBIOGET_VSCREENINFO");
                exit(1);
        }

        if (ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix) == -1) {
                perror("ioctl FBIOGET_FSCREENINFO");
                exit(1);
        }

        if ((fb_ovar.bits_per_pixel == 8) ||
            (fb_fix.visual == FB_VISUAL_DIRECTCOLOR)) {
                if (ioctl(fb, FBIOGETCMAP, &ocmap) == -1) {
                        perror("ioctl FBIOGETCMAP");
                        exit(1);
                }
        }

        if (ioctl(tty, KDGETMODE, &kd_mode) == -1) {
                perror("ioctl KDGETMODE");
                exit(1);
        }

        if (ioctl(tty,VT_GETMODE, &vt_omode) == -1) {
                perror("ioctl VT_GETMODE");
                exit(1);
        }
        tcgetattr(tty, &term);

        /* switch mode */
        fb_setmode(mode, bpp);

        /* checks & initialisation */
        if (ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix) == -1) {
                perror("ioctl FBIOGET_FSCREENINFO");
                exit(1);
        }
        if (fb_fix.type != FB_TYPE_PACKED_PIXELS) {
                fprintf(stderr,"can handle only packed pixel frame buffers\n");
                goto err;
        }

        fb_mem_offset = (unsigned long)(fb_fix.smem_start) & (~pm);
        fb_mem = mmap(NULL, fb_fix.smem_len + fb_mem_offset,
                      PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);

        if (-1L == (long)fb_mem) {
                perror("mmap");
                goto err;
        }

        /* move viewport to upper left corner */
        if ((fb_var.xoffset != 0) || 
            (fb_var.yoffset != 0)) {
                fb_var.xoffset = 0;
                fb_var.yoffset = 0;
                if (ioctl(fb, FBIOPAN_DISPLAY, &fb_var) == -1) {
                        perror("ioctl FBIOPAN_DISPLAY");
                        goto err;
                }
        }

        if (ioctl(tty, KDSETMODE, KD_GRAPHICS) == -1) {
                perror("ioctl KDSETMODE");
                goto err;
        }
        fb_activate_current(tty);

        /* cls */
        memset(fb_mem + fb_mem_offset, 0, fb_fix.smem_len);
        return fb;

err:
        fb_cleanup();
        exit(1);
}


/* -------------------------------------------------------------------- */
/* handle fatal errors                                                  */

static jmp_buf fb_fatal_cleanup;

static void
fb_catch_exit_signal(int signal)
{
        siglongjmp(fb_fatal_cleanup,signal);
}

static void
fb_catch_exit_signals(void)
{
        struct sigaction act,old;
        int termsig;

        memset(&act,0,sizeof(act));
        act.sa_handler = fb_catch_exit_signal;
        sigemptyset(&act.sa_mask);
        sigaction(SIGINT, &act, &old);
        sigaction(SIGQUIT, &act, &old);
        sigaction(SIGTERM, &act, &old);

        sigaction(SIGABRT, &act, &old);
        sigaction(SIGTSTP, &act, &old);

        sigaction(SIGBUS, &act, &old);
        sigaction(SIGILL, &act, &old);
        sigaction(SIGSEGV, &act, &old);

        if ((termsig = sigsetjmp(fb_fatal_cleanup,0)) == 0)
                return;

        /* cleanup */
        fb_cleanup();
        fprintf(stderr, "Oops: %s\n", sys_siglist[termsig]);
        exit(42);
}

framebuffer_t *fb_os_init(int argc, char** argv)
{
        framebuffer_t *newfb;
        int ploop;
        int fb_width;
        int fb_height;
        int fb_refresh;
        int fb_depth;

        if ((option_window_width != 0) && 
            (option_window_height != 0)) {
                fb_width = option_window_width;
                fb_height = option_window_height;
        } else {
                fb_width = 800;
                fb_height = 600;
        }

        if (option_fb_refresh != 0) {
                fb_refresh = option_fb_refresh;
        } else {
                fb_refresh = 60;
        }

        fb_depth = option_fb_depth;
        if ((fb_depth != 32) && 
            (fb_depth != 16) && 
            (fb_depth != 8))
                fb_depth = 16; /* sanity checked depth in bpp */

        framebuffer_init(option_fb_device, fb_width, fb_height, fb_refresh, fb_depth, 1);
        fb_switch_init();
        fb_catch_exit_signals();

        newfb = calloc(1, sizeof(framebuffer_t));

        newfb->width = fb_var.xres;
        newfb->height = fb_var.yres;
        newfb->ptr = fb_mem;
        newfb->linelen = fb_fix.line_length;
        newfb->bpp = fb_var.bits_per_pixel;

        if (newfb->bpp <= 8) {
                for(ploop=0; ploop < 256; ploop++) {
                        newfb->palette[ploop] = 0xFF000000 |
                                                ocmap.blue[ploop] << 16 |
                                                ocmap.green[ploop] << 8 |
                                                ocmap.red[ploop] ;
                }
        }


        fb_open_input_devices();

        return newfb;
}

void fb_os_quit(framebuffer_t *fb)
{
        fb_cleanup();
}


static int keymap[] = {
        -1,  -1, '1',  '2', '3', '4', '5', '6', '7', '8', /*  0 -  9 */
        '9', '0', '-',  '=',   8,   9, 'q', 'w', 'e', 'r', /* 10 - 19 */
        't', 'y', 'u',  'i', 'o', 'p', '[', ']',  13,  -1, /* 20 - 29 */
        'a', 's', 'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', /* 30 - 39 */
        '\'', '#', -1, '\\', 'z', 'x', 'c', 'v', 'b', 'n', /* 40 - 49 */
        'm', ',', '.',  '/',  -1,  -1,  -1, ' ',  -1,  -1, /* 50 - 59 */
};

static int sh_keymap[] = {
        -1,  -1, '!', '"', 0xa3, '$', '%', '^', '&', '*', /*  0 -  9 */
        '(', ')', '_', '+',    8,   9, 'Q', 'W', 'E', 'R', /* 10 - 19 */
        'T', 'Y', 'U', 'I',  'O', 'P', '{', '}',  13,  -1, /* 20 - 29 */
        'A', 'S', 'D', 'F',  'G', 'H', 'J', 'K', 'L', ':', /* 30 - 39 */
        '@', '~',  -1, '|',  'Z', 'X', 'C', 'V', 'B', 'N', /* 40 - 49 */
        'M', '<', '>', '?',   -1,  -1,  -1, ' ',  -1,  -1, /* 50 - 59 */
};


/* performs character mapping */
static int keycode_to_ucs4(int code, bool shift)
{
        int ucs4 = -1;

        if (shift) {
                if ((code >= 0) && (code < sizeof(sh_keymap)))
                        ucs4 = sh_keymap[code];
        } else {
                if ((code >= 0) && (code < sizeof(keymap)))
                        ucs4 = keymap[code];
        }
        return ucs4;
}

void fb_os_input(fbtk_widget_t *root, bool active)
{
        ssize_t amt;
        struct input_event event;
        fb_input_dev *d;
        int ucs4 = -1;
        static bool shift = false;

        for (d = inputdevs; d != NULL; d = d->next) {
                amt = read(d->fd, &event, sizeof(struct input_event));

                if (amt > 0) {
                        if (event.type == EV_KEY) {
                                if (event.value == 0) {
                                        /* key up */
                                        switch (event.code) {
                                        case KEY_LEFTSHIFT:
                                        case KEY_RIGHTSHIFT:
                                                shift = false;
                                                break;

                                        case BTN_LEFT:
                                                fbtk_click(root, BROWSER_MOUSE_CLICK_1);
                                                break;
                                        }
                                        return;
                                }

                                switch (event.code) {
                                case KEY_PAGEDOWN:
                                        ucs4 = NSKEY_PAGE_DOWN;
                                        break;

                                case KEY_PAGEUP:
                                        ucs4 = NSKEY_PAGE_UP;
                                        break;

                                case KEY_DOWN:
                                        ucs4 = NSKEY_DOWN;
                                        break;

                                case KEY_UP:
                                        ucs4 = NSKEY_UP;
                                        break;

                                case KEY_LEFT:
                                        ucs4 = NSKEY_LEFT;
                                        break;

                                case KEY_RIGHT:
                                        ucs4 = NSKEY_RIGHT;
                                        break;

                                case KEY_ESC:
                                        ucs4 = NSKEY_ESCAPE;
                                        break;

                                case BTN_LEFT:
                                        fbtk_click(root, BROWSER_MOUSE_PRESS_1);
                                        break;

                                case KEY_LEFTSHIFT:
                                case KEY_RIGHTSHIFT:
                                        shift = true;
                                        break;

                                default:
                                        ucs4 = keycode_to_ucs4(event.code, shift);

                                }
                        } else if (event.type == EV_REL) {
                                switch (event.code) {
                                case REL_X:
                                        fbtk_move_pointer(root, event.value, 0, true);
                                        break;

                                case REL_Y:
                                        fbtk_move_pointer(root, 0, event.value, true);
                                        break;

                                case REL_WHEEL:
                                        if (event.value > 0)
                                                fbtk_input(root, NSKEY_UP);
                                        else
                                                fbtk_input(root, NSKEY_DOWN);
                                        break;
                                }
                        } else if (event.type == EV_ABS) {
                                switch (event.code) {
                                case ABS_X:
                                        fbtk_move_pointer(root, event.value, -1, false);
                                        break;

                                case ABS_Y:
                                        fbtk_move_pointer(root, -1, event.value, false);
                                        break;

                                }
                        }

                        if (ucs4 != -1) {
                                fbtk_input(root, ucs4);
                                ucs4 = -1;
                        }


                }
        }
}

void
fb_os_option_override(void)
{
}

/* called by generic code to inform os code of screen update */
void
fb_os_redraw(struct bbox_s *box)
{
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
