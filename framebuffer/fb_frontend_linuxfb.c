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
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "framebuffer/fb_gui.h"
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


static int                       fb,tty;
static int                       orig_vt_no = 0;
static struct vt_mode            vt_mode;

static int                       kd_mode;
static struct vt_mode            vt_omode;
static struct termios            term;
static struct fb_var_screeninfo  fb_ovar;
static unsigned short            ored[256], ogreen[256], oblue[256], otransp[256];
static struct fb_cmap            ocmap = { 0, 256, ored, ogreen, oblue, otransp };

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

struct DEVS devs_devfs = {
        .fb0 = "/dev/fb/0",
        .fbnr = "/dev/fb/%d",
        .ttynr = "/dev/vc/%d",
};
struct DEVS *devices;

static void dev_init(void)
{
        struct stat dummy;

        if (NULL != devices)
                return;
        if (0 == stat("/dev/.devfsd",&dummy))
                devices = &devs_devfs;
        else
                devices = &devs_default;
}

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
                if (fnmatch(option_fb_input_glob ? option_fb_input_glob : "event*",
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

        memset(&act,0,sizeof(act));
        act.sa_handler  = fb_switch_signal;
        sigemptyset(&act.sa_mask);
        sigaction(SIGUSR1,&act,&old);
        sigaction(SIGUSR2,&act,&old);
    
        if (-1 == ioctl(tty,VT_GETMODE, &vt_mode)) {
                perror("ioctl VT_GETMODE");
                exit(1);
        }
        vt_mode.mode   = VT_PROCESS;
        vt_mode.waitv  = 0;
        vt_mode.relsig = SIGUSR1;
        vt_mode.acqsig = SIGUSR2;
    
        if (-1 == ioctl(tty,VT_SETMODE, &vt_mode)) {
                perror("ioctl VT_SETMODE");
                exit(1);
        }
        return 0;
}

/* -------------------------------------------------------------------- */
/* initialisation & cleanup                                             */

static void
fb_memset (void *addr, int c, size_t len)
{
#if 1 /* defined(__powerpc__) */
        unsigned int i, *p;
    
        i = (c & 0xff) << 8;
        i |= i << 16;
        len >>= 2;
        for (p = addr; len--; p++)
                *p = i;
#else
        memset(addr, c, len);
#endif
}

static int
fb_setmode(const char *name, int bpp)
{
        FILE *fp;
        char line[80],label[32],value[16];
        int  geometry=0, timings=0;
    
        /* load current values */
        if (ioctl(fb,FBIOGET_VSCREENINFO,&fb_var) == -1) {
                perror("ioctl FBIOGET_VSCREENINFO");
                exit(1);
        }
    
        if (name == NULL)
                return -1;
        if ((fp = fopen("/etc/fb.modes","r")) == NULL)
                return -1;
        while (NULL != fgets(line,79,fp)) {
                if (1 == sscanf(line, "mode \"%31[^\"]\"",label) &&
                    0 == strcmp(label,name)) {
                        /* fill in new values */
                        fb_var.sync  = 0;
                        fb_var.vmode = 0;
                        while (NULL != fgets(line,79,fp) &&
                               NULL == strstr(line,"endmode")) {
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
                        if (-1 == ioctl(fb,FBIOPUT_VSCREENINFO,&fb_var))
                                perror("ioctl FBIOPUT_VSCREENINFO");
                        /* look what we have now ... */
                        if (-1 == ioctl(fb,FBIOGET_VSCREENINFO,&fb_var)) {
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
                if (-1 == ioctl(tty,VT_OPENQRY, &vtno) || vtno == -1) {
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
        /*        switch (fork()) {
        case 0:
                break;
        case -1:
                perror("fork");
                exit(1);
        default:
                exit(0);
                }
        close(tty);
        close(0);
        close(1);
        close(2);
        setsid();
        open(vtname,O_RDWR);
        dup(0);
        dup(0);
*/
        tty = open(vtname,O_RDWR);
        if (-1 == ioctl(tty,VT_GETSTATE, &vts)) {
                perror("ioctl VT_GETSTATE");
                exit(1);
        }
        orig_vt_no = vts.v_active;
        if (-1 == ioctl(tty,VT_ACTIVATE, vtno)) {
                perror("ioctl VT_ACTIVATE");
                exit(1);
        }
        if (-1 == ioctl(tty,VT_WAITACTIVE, vtno)) {
                perror("ioctl VT_WAITACTIVE");
                exit(1);
        }
}

/* Hmm. radeonfb needs this. matroxfb doesn't. */
static int 
fb_activate_current(int tty)
{
        struct vt_stat vts;
    
        if (-1 == ioctl(tty,VT_GETSTATE, &vts)) {
                perror("ioctl VT_GETSTATE");
                return -1;
        }
        if (-1 == ioctl(tty,VT_ACTIVATE, vts.v_active)) {
                perror("ioctl VT_ACTIVATE");
                return -1;
        }
        if (-1 == ioctl(tty,VT_WAITACTIVE, vts.v_active)) {
                perror("ioctl VT_WAITACTIVE");
                return -1;
        }
        return 0;
}

static void
fb_cleanup(void)
{
        /* restore console */
        if (-1 == ioctl(fb,FBIOPUT_VSCREENINFO,&fb_ovar))
                perror("ioctl FBIOPUT_VSCREENINFO");
        if (-1 == ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix))
                perror("ioctl FBIOGET_FSCREENINFO");
        if (fb_ovar.bits_per_pixel == 8 ||
            fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
                if (-1 == ioctl(fb,FBIOPUTCMAP,&ocmap))
                        perror("ioctl FBIOPUTCMAP");
        }
        close(fb);

        if (-1 == ioctl(tty,KDSETMODE, kd_mode))
                perror("ioctl KDSETMODE");
        if (-1 == ioctl(tty,VT_SETMODE, &vt_omode))
                perror("ioctl VT_SETMODE");
        if (orig_vt_no && -1 == ioctl(tty, VT_ACTIVATE, orig_vt_no))
                perror("ioctl VT_ACTIVATE");
        if (orig_vt_no && -1 == ioctl(tty, VT_WAITACTIVE, orig_vt_no))
                perror("ioctl VT_WAITACTIVE");
        tcsetattr(tty, TCSANOW, &term);
        close(tty);
}

static int
fb_init(const char *device, const char *mode, int bpp, int vt)
{
        char   fbdev[16];
        struct vt_stat vts;
        long pm = ~(sysconf(_SC_PAGESIZE) - 1);

        dev_init();
        tty = 0;
        if (vt != 0)
                fb_setvt(vt);

        if (-1 == ioctl(tty,VT_GETSTATE, &vts)) {
                fprintf(stderr,"ioctl VT_GETSTATE: %s (not a linux console?)\n",
                        strerror(errno));
                exit(1);
        }
    
        if (device == NULL) {
                device = getenv("FRAMEBUFFER");
                if (device == NULL) {
                        struct fb_con2fbmap c2m;
                        if (-1 == (fb = open(devices->fb0,O_RDWR /* O_WRONLY */,0))) {
                                fprintf(stderr,"open %s: %s\n",devices->fb0,strerror(errno));
                                exit(1);
                        }
                        c2m.console = vts.v_active;
                        if (-1 == ioctl(fb, FBIOGET_CON2FBMAP, &c2m)) {
                                perror("ioctl FBIOGET_CON2FBMAP");
                                exit(1);
                        }
                        close(fb);
                        fprintf(stderr,"map: vt%02d => fb%d\n",
                                c2m.console,c2m.framebuffer);
                        sprintf(fbdev,devices->fbnr,c2m.framebuffer);
                        device = fbdev;
                }
        }

        /* get current settings (which we have to restore) */
        if (-1 == (fb = open(device,O_RDWR /* O_WRONLY */))) {
                fprintf(stderr,"open %s: %s\n",device,strerror(errno));
                exit(1);
        }
        if (-1 == ioctl(fb,FBIOGET_VSCREENINFO,&fb_ovar)) {
                perror("ioctl FBIOGET_VSCREENINFO");
                exit(1);
        }
        if (-1 == ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix)) {
                perror("ioctl FBIOGET_FSCREENINFO");
                exit(1);
        }
        if (fb_ovar.bits_per_pixel == 8 ||
            fb_fix.visual == FB_VISUAL_DIRECTCOLOR) {
                if (-1 == ioctl(fb,FBIOGETCMAP,&ocmap)) {
                        perror("ioctl FBIOGETCMAP");
                        exit(1);
                }
        }
        if (-1 == ioctl(tty,KDGETMODE, &kd_mode)) {
                perror("ioctl KDGETMODE");
                exit(1);
        }
        if (-1 == ioctl(tty,VT_GETMODE, &vt_omode)) {
                perror("ioctl VT_GETMODE");
                exit(1);
        }
        tcgetattr(tty, &term);
    
        /* switch mode */
        fb_setmode(mode, bpp);
    
        /* checks & initialisation */
        if (-1 == ioctl(fb,FBIOGET_FSCREENINFO,&fb_fix)) {
                perror("ioctl FBIOGET_FSCREENINFO");
                exit(1);
        }
        if (fb_fix.type != FB_TYPE_PACKED_PIXELS) {
                fprintf(stderr,"can handle only packed pixel frame buffers\n");
                goto err;
        }

        fb_mem_offset = (unsigned long)(fb_fix.smem_start) & (~pm);
        fb_mem = mmap(NULL,fb_fix.smem_len+fb_mem_offset,
                      PROT_READ|PROT_WRITE,MAP_SHARED,fb,0);
        if (-1L == (long)fb_mem) {
                perror("mmap");
                goto err;
        }
        /* move viewport to upper left corner */
        if (fb_var.xoffset != 0 || fb_var.yoffset != 0) {
                fb_var.xoffset = 0;
                fb_var.yoffset = 0;
                if (-1 == ioctl(fb,FBIOPAN_DISPLAY,&fb_var)) {
                        perror("ioctl FBIOPAN_DISPLAY");
                        goto err;
                }
        }
        if (-1 == ioctl(tty,KDSETMODE, KD_GRAPHICS)) {
                perror("ioctl KDSETMODE");
                goto err;
        }
        fb_activate_current(tty);

        /* cls */
        fb_memset(fb_mem + fb_mem_offset, 0, fb_fix.smem_len);
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
        sigaction(SIGINT, &act,&old);
        sigaction(SIGQUIT,&act,&old);
        sigaction(SIGTERM,&act,&old);

        sigaction(SIGABRT,&act,&old);
        sigaction(SIGTSTP,&act,&old);

        sigaction(SIGBUS, &act,&old);
        sigaction(SIGILL, &act,&old);
        sigaction(SIGSEGV,&act,&old);

        if (0 == (termsig = sigsetjmp(fb_fatal_cleanup,0)))
                return;

        /* cleanup */
        fb_cleanup();
        fprintf(stderr,"Oops: %s\n",sys_siglist[termsig]);
        exit(42);
}

framebuffer_t *fb_os_init(int argc, char** argv)
{
        framebuffer_t *newfb;
        int ploop;

        fb_init(option_fb_device, option_fb_mode ? option_fb_mode : "800x600-70", 16, 1);
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

void fb_os_input(struct gui_window *g, bool active) 
{
        ssize_t amt;
        struct input_event event;       
        fb_input_dev *d;
        
        for (d = inputdevs; d != NULL; d = d->next) {
                amt = read(d->fd, &event, sizeof(struct input_event));
                
                if (amt > 0) {
                        if (event.type == EV_KEY) {
                                if (event.value == 0)
                                        return;
                                
                                switch (event.code) {
                                case KEY_J:
                                        fb_window_scroll(g, 0, 100);
                                        break;
                                        
                                case KEY_K:
                                        fb_window_scroll(g, 0, -100);
                                        break;
                                        
                                case KEY_Q:
                                        browser_window_destroy(g->bw);
                                        break;
                                        
                                case KEY_D:
                                        list_schedule();
                                        break;
                                        
                                case KEY_UP:
                                        fb_cursor_move(framebuffer, 0, -1);
                                        break;
                                        
                                case KEY_DOWN:
                                        fb_cursor_move(framebuffer, 0, 1);
                                        break;
                                        
                                case KEY_LEFT:
                                        fb_cursor_move(framebuffer, -1, 0);
                                        break;
                                        
                                case KEY_RIGHT:
                                        fb_cursor_move(framebuffer, 1, 0);
                                        break;
                                case BTN_LEFT:
                                        fb_cursor_click(framebuffer,
                                                        g, 
                                                        BROWSER_MOUSE_CLICK_1);
                                        break;
                                }
                        } else if (event.type == EV_REL) {
                                switch (event.code) {
				case 0:
                                        fb_cursor_move(framebuffer, 
                                                       event.value, 
                                                       0);
					break;

                                case 1: 
                                        fb_cursor_move(framebuffer, 
                                                       0, 
                                                       event.value);
					break;

				case 8:
					fb_window_scroll(g, 0, event.value * -100);
					break;
                                }
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

