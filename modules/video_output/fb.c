/*****************************************************************************
 * fb.c : framebuffer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2009 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Jean-Paul Saman
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <signal.h>                                      /* SIGUSR1, SIGUSR2 */
#include <fcntl.h>                                                 /* open() */
#include <unistd.h>                                               /* close() */
#include <errno.h>
#include <termios.h>                                       /* struct termios */
#include <sys/ioctl.h>
#include <sys/mman.h>                                              /* mmap() */

#include <linux/fb.h>
#include <linux/vt.h>                                                /* VT_* */
#include <linux/kd.h>                                                 /* KD* */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_fs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FB_DEV_VAR "fbdev"

#define DEVICE_TEXT N_("Framebuffer device")
#define DEVICE_LONGTEXT N_(\
    "Framebuffer device to use for rendering (usually /dev/fb0).")

#define TTY_TEXT N_("Run fb on current tty")
#define TTY_LONGTEXT N_(\
    "Run framebuffer on current TTY device (default enabled). " \
    "(disable tty handling with caution)")

#define FB_MODE_TEXT N_("Framebuffer resolution to use")
#define FB_MODE_LONGTEXT N_(\
    "Select the resolution for the framebuffer. Currently it supports " \
    "the values 0=QCIF 1=CIF 2=NTSC 3=PAL, 4=auto (default 4=auto)")

#define HW_ACCEL_TEXT N_("Framebuffer uses hw acceleration")
#define HW_ACCEL_LONGTEXT N_("Disable for double buffering in software.")

#define CHROMA_TEXT N_("Image format (default RGB)")
#define CHROMA_LONGTEXT N_("Chroma fourcc used by the framebuffer. Default is RGB since the fb device has no way to report its chroma.")

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context);
static void Close(vout_display_t *vd);

vlc_module_begin ()
    set_shortname("Framebuffer")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_loadfile(FB_DEV_VAR, "/dev/fb0", DEVICE_TEXT, DEVICE_LONGTEXT)
    add_bool("fb-tty", true, TTY_TEXT, TTY_LONGTEXT, true)
    add_string( "fb-chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true )
    add_obsolete_string("fb-aspect-ratio")
    add_integer("fb-mode", 4, FB_MODE_TEXT, FB_MODE_LONGTEXT,
                 true)
    add_obsolete_bool("fb-hw-accel") /* since 4.0.0 */
    set_description(N_("GNU/Linux framebuffer video output"))
    set_callback_display(Open, 30)
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);

/* */
static int  OpenDisplay  (vout_display_t *, bool force_resolution);
static void CloseDisplay (vout_display_t *);
#if 0
static void SwitchDisplay(int i_signal);
#endif
static void TextMode     (int tty);
static void GfxMode      (int tty);

static int  TtyInit(vout_display_t *);
static void TtyExit(vout_display_t *);

/* */
struct vout_display_sys_t {
    /* System information */
    int                 tty;                          /* tty device handle */
    bool                is_tty;
    struct termios      old_termios;

    /* Original configuration information */
#if 0
    struct sigaction            sig_usr1;           /* USR1 previous handler */
    struct sigaction            sig_usr2;           /* USR2 previous handler */
#endif
    struct vt_mode              vt_mode;                 /* previous VT mode */

    /* Framebuffer information */
    int                         fd;                       /* device handle */
    struct fb_var_screeninfo    old_info;       /* original mode information */
    struct fb_var_screeninfo    var_info;        /* current mode information */
    bool                        has_pan;   /* does device supports panning ? */
    struct fb_cmap              fb_cmap;                /* original colormap */
    uint16_t                    *palette;                /* original palette */

    /* Video information */
    uint32_t width;
    uint32_t height;
    uint32_t line_length;
    vlc_fourcc_t chroma;
    int      bytes_per_pixel;

    /* Video memory */
    uint8_t     *video_ptr;                                 /* base address */
    size_t      video_size;                                    /* page size */

    picture_t       *picture;
};


static void ClearScreen(vout_display_sys_t *sys)
{
    switch (sys->chroma) {
    /* XXX: add other chromas */
    case VLC_CODEC_UYVY: {
        unsigned int j, size = sys->video_size / 4;
        uint32_t *ptr = (uint32_t*)((uintptr_t)(sys->video_ptr + 3) & ~3);
        for(j=0; j < size; j++)
            ptr[j] = 0x10801080;    /* U = V = 16, Y = 128 */
        break;
    }
    default:    /* RGB */
        memset(sys->video_ptr, 0, sys->video_size);
    }
}

/**
 * This function allocates and initializes a FB vout method.
 */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;

    if (vout_display_cfg_IsWindowed(cfg))
        return VLC_EGENERIC;

    /* Allocate instance and initialize some members */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->fd = -1;

    /* Set tty and fb devices */
    sys->tty = 0; /* 0 == /dev/tty0 == current console */
    sys->is_tty = var_InheritBool(vd, "fb-tty");
#if !defined(_WIN32) &&  defined(HAVE_ISATTY)
    /* Check that stdin is a TTY */
    if (sys->is_tty && !isatty(0)) {
        msg_Warn(vd, "standard input is not a TTY");
        free(sys);
        return VLC_EGENERIC;
    }
    msg_Warn(vd, "disabling TTY handling, use with caution because "
                 "there is no way to return to the TTY");
#endif

    const int mode = var_InheritInteger(vd, "fb-mode");
    bool force_resolution = true;
    switch (mode) {
    case 0: /* QCIF */
        sys->width  = 176;
        sys->height = 144;
        break;
    case 1: /* CIF */
        sys->width  = 352;
        sys->height = 288;
        break;
    case 2: /* NTSC */
        sys->width  = 640;
        sys->height = 480;
        break;
    case 3: /* PAL */
        sys->width  = 704;
        sys->height = 576;
        break;
    case 4:
    default:
        force_resolution = false;
        break;
    }

    char *chroma = var_InheritString(vd, "fb-chroma");
    if (chroma) {
        sys->chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);

        if (sys->chroma)
            msg_Dbg(vd, "forcing chroma '%s'", chroma);
        else
            msg_Warn(vd, "chroma %s invalid, using default", chroma);

        free(chroma);
    } else
        sys->chroma = 0;

    /* tty handling */
    if (sys->is_tty && TtyInit(vd)) {
        free(sys);
        return VLC_EGENERIC;
    }

    /* */
    sys->video_ptr = MAP_FAILED;
    sys->picture = NULL;

    if (OpenDisplay(vd, force_resolution)) {
        Close(vd);
        return VLC_EGENERIC;
    }

    /* */
    video_format_t fmt;
    video_format_ApplyRotation(&fmt, fmtp);

    if (sys->chroma) {
        fmt.i_chroma = sys->chroma;
    } else {
        /* Assume RGB */

        msg_Dbg(vd, "%d bppd", sys->var_info.bits_per_pixel);
        switch (sys->var_info.bits_per_pixel) {
        case 8: /* FIXME: set the palette */
            fmt.i_chroma = VLC_CODEC_RGB8;
            break;
        case 15:
            fmt.i_chroma = VLC_CODEC_RGB15;
            break;
        case 16:
            fmt.i_chroma = VLC_CODEC_RGB16;
            break;
        case 24:
            fmt.i_chroma = VLC_CODEC_RGB24;
            break;
        case 32:
            fmt.i_chroma = VLC_CODEC_RGB32;
            break;
        default:
            msg_Err(vd, "unknown screendepth %i", sys->var_info.bits_per_pixel);
            Close(vd);
            return VLC_EGENERIC;
        }
        if (sys->var_info.bits_per_pixel != 8) {
            fmt.i_rmask = ((1 << sys->var_info.red.length) - 1)
                                 << sys->var_info.red.offset;
            fmt.i_gmask = ((1 << sys->var_info.green.length) - 1)
                                 << sys->var_info.green.offset;
            fmt.i_bmask = ((1 << sys->var_info.blue.length) - 1)
                                 << sys->var_info.blue.offset;
        }
    }

    fmt.i_visible_width  = sys->width;
    fmt.i_visible_height = sys->height;

    /* */
    *fmtp = fmt;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->close = Close;

    (void) context;
    return VLC_SUCCESS;
}

/**
 * Terminate an output method created by Open
 */
static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->picture != NULL)
        picture_Release(sys->picture);

    CloseDisplay(vd);

    if (sys->is_tty)
        TtyExit(vd);

    free(sys);
}

/* */
static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    /* swap the two Y offsets if the drivers supports panning */
    if (sys->has_pan) {
        sys->var_info.yoffset = 0;
        /*vd->sys->var_info.yoffset = vd->sys->var_info.yres; */

        /* the X offset should be 0, but who knows ...
         * some other app might have played with the framebuffer */
        sys->var_info.xoffset = 0;

        /* FIXME 'static' is damn wrong and it's dead code ... */
        static int panned = 0;
        if (panned < 0) {
            ioctl(sys->fd, FBIOPAN_DISPLAY, &sys->var_info);
            panned++;
        }
    }

    picture_Copy(sys->picture, picture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    (void) vd; (void) args;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/* following functions are local */
static int TtyInit(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    struct termios new_termios;

    GfxMode(sys->tty);

    /* Set keyboard settings */
    if (tcgetattr(0, &sys->old_termios) == -1) {
        msg_Err(vd, "tcgetattr failed");
    }

    if (tcgetattr(0, &new_termios) == -1) {
        msg_Err(vd, "tcgetattr failed");
    }

    /* new_termios.c_lflag &= ~ (ICANON | ISIG);
    new_termios.c_lflag |= (ECHO | ECHOCTL); */
    new_termios.c_lflag &= ~ (ICANON);
    new_termios.c_lflag &= ~(ECHO | ECHOCTL);
    new_termios.c_iflag = 0;
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSAFLUSH, &new_termios) == -1) {
        msg_Err(vd, "tcsetattr failed");
    }

    ioctl(sys->tty, VT_RELDISP, VT_ACKACQ);

#if 0
    /* Set-up tty signal handler to be aware of tty changes */
    struct sigaction sig_tty;
    memset(&sig_tty, 0, sizeof(sig_tty));
    sig_tty.sa_handler = SwitchDisplay;
    sigemptyset(&sig_tty.sa_mask);
    if (sigaction(SIGUSR1, &sig_tty, &sys->sig_usr1) ||
        sigaction(SIGUSR2, &sig_tty, &sys->sig_usr2)) {
        msg_Err(vd, "cannot set signal handler (%s)", vlc_strerror_c(errno));
        /* FIXME SIGUSR1 could have succeed */
        goto error_signal;
    }
#endif

    /* Set-up tty according to new signal handler */
    if (-1 == ioctl(sys->tty, VT_GETMODE, &sys->vt_mode)) {
        msg_Err(vd, "cannot get terminal mode (%s)", vlc_strerror_c(errno));
        goto error;
    }
    struct vt_mode vt_mode = sys->vt_mode;
    vt_mode.mode   = VT_PROCESS;
    vt_mode.waitv  = 0;
    vt_mode.relsig = SIGUSR1;
    vt_mode.acqsig = SIGUSR2;

    if (-1 == ioctl(sys->tty, VT_SETMODE, &vt_mode)) {
        msg_Err(vd, "cannot set terminal mode (%s)", vlc_strerror_c(errno));
        goto error;
    }
    return VLC_SUCCESS;

error:
#if 0
    sigaction(SIGUSR1, &sys->sig_usr1, NULL);
    sigaction(SIGUSR2, &sys->sig_usr2, NULL);
error_signal:
#endif
    tcsetattr(0, 0, &sys->old_termios);
    TextMode(sys->tty);
    return VLC_EGENERIC;
}
static void TtyExit(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /* Reset the terminal */
    ioctl(sys->tty, VT_SETMODE, &sys->vt_mode);

#if 0
    /* Remove signal handlers */
    sigaction(SIGUSR1, &sys->sig_usr1, NULL);
    sigaction(SIGUSR2, &sys->sig_usr2, NULL);
#endif

    /* Reset the keyboard state */
    tcsetattr(0, 0, &sys->old_termios);

    /* Return to text mode */
    TextMode(sys->tty);
}

/*****************************************************************************
 * OpenDisplay: initialize framebuffer
 *****************************************************************************/
static int OpenDisplay(vout_display_t *vd, bool force_resolution)
{
    vout_display_sys_t *sys = vd->sys;
    char *psz_device;                             /* framebuffer device path */

    /* Open framebuffer device */
    if (!(psz_device = var_InheritString(vd, FB_DEV_VAR))) {
        msg_Err(vd, "don't know which fb device to open");
        return VLC_EGENERIC;
    }

    sys->fd = vlc_open(psz_device, O_RDWR);
    if (sys->fd == -1) {
        msg_Err(vd, "cannot open %s (%s)", psz_device, vlc_strerror_c(errno));
        free(psz_device);
        return VLC_EGENERIC;
    }
    free(psz_device);

    /* Get framebuffer device information */
    if (ioctl(sys->fd, FBIOGET_VSCREENINFO, &sys->var_info)) {
        msg_Err(vd, "cannot get fb info (%s)", vlc_strerror_c(errno));
        return VLC_EGENERIC;
    }
    sys->old_info = sys->var_info;

    /* Get some info on the framebuffer itself */
    if (force_resolution) {
        sys->var_info.xres = sys->var_info.xres_virtual = sys->width;
        sys->var_info.yres = sys->var_info.yres_virtual = sys->height;
    }

    /* Set some attributes */
    sys->var_info.activate = sys->is_tty ? FB_ACTIVATE_NXTOPEN :
                                           FB_ACTIVATE_NOW;
    sys->var_info.xoffset  =  0;
    sys->var_info.yoffset  =  0;

    if (ioctl(sys->fd, FBIOPUT_VSCREENINFO, &sys->var_info)) {
        msg_Err(vd, "cannot set fb info (%s)", vlc_strerror_c(errno));
        return VLC_EGENERIC;
    }

    struct fb_fix_screeninfo fix_info;
    /* Get some information again, in the definitive configuration */
    if (ioctl(sys->fd, FBIOGET_FSCREENINFO, &fix_info) ||
        ioctl(sys->fd, FBIOGET_VSCREENINFO, &sys->var_info)) {
        msg_Err(vd, "cannot get additional fb info (%s)",
                vlc_strerror_c(errno));

        /* Restore fb config */
        ioctl(sys->fd, FBIOPUT_VSCREENINFO, &sys->old_info);
        return VLC_EGENERIC;
    }

    /* If the fb has limitations on mode change,
     * then keep the resolution of the fb */
    if ((sys->height != sys->var_info.yres) ||
        (sys->width != sys->var_info.xres)) {
        msg_Warn(vd,
                 "using framebuffer native resolution instead of requested (%ix%i)",
                 sys->width, sys->height);
    }
    sys->height = sys->var_info.yres;
    sys->width  = sys->var_info.xres_virtual ? sys->var_info.xres_virtual :
                                               sys->var_info.xres;
    sys->line_length = fix_info.line_length;

    /* FIXME: if the image is full-size, it gets cropped on the left
     * because of the xres / xres_virtual slight difference */
    msg_Dbg(vd, "%ix%i (virtual %ix%i) (request %ix%i)",
            sys->var_info.xres, sys->var_info.yres,
            sys->var_info.xres_virtual,
            sys->var_info.yres_virtual,
            sys->width, sys->height);

    sys->palette = NULL;
    sys->has_pan = (fix_info.ypanstep || fix_info.ywrapstep);

    switch (sys->var_info.bits_per_pixel) {
    case 8:
        sys->palette = malloc(4 * 256 * sizeof(uint16_t));
        if (!sys->palette) {
            /* Restore fb config */
            ioctl(sys->fd, FBIOPUT_VSCREENINFO, &sys->old_info);
            return VLC_ENOMEM;
        }
        sys->fb_cmap.start = 0;
        sys->fb_cmap.len = 256;
        sys->fb_cmap.red = sys->palette;
        sys->fb_cmap.green = sys->palette + 256;
        sys->fb_cmap.blue = sys->palette + 2 * 256;
        sys->fb_cmap.transp = sys->palette + 3 * 256;

        /* Save the colormap */
        ioctl(sys->fd, FBIOGETCMAP, &sys->fb_cmap);

        sys->bytes_per_pixel = 1;
        break;

    case 15:
    case 16:
        sys->bytes_per_pixel = 2;
        break;

    case 24:
        sys->bytes_per_pixel = 3;
        break;

    case 32:
        sys->bytes_per_pixel = 4;
        break;

    default:
        msg_Err(vd, "screen depth %d is not supported",
                sys->var_info.bits_per_pixel);

        /* Restore fb config */
        ioctl(sys->fd, FBIOPUT_VSCREENINFO, &sys->old_info);
        return VLC_EGENERIC;
    }

    sys->video_size = sys->line_length * sys->var_info.yres_virtual;

    /* Map a framebuffer at the beginning */
    sys->video_ptr = mmap(NULL, sys->video_size,
                          PROT_READ | PROT_WRITE, MAP_SHARED, sys->fd, 0);

    if (sys->video_ptr == MAP_FAILED) {
        msg_Err(vd, "cannot map video memory (%s)", vlc_strerror_c(errno));

        if (sys->var_info.bits_per_pixel == 8) {
            free(sys->palette);
            sys->palette = NULL;
        }

        /* Restore fb config */
        ioctl(sys->fd, FBIOPUT_VSCREENINFO, &sys->old_info);
        return VLC_EGENERIC;
    }

    picture_resource_t rsc = {
       .p = {
           [0] = {
               .p_pixels = sys->video_ptr,
               .i_lines = sys->var_info.yres,
               .i_pitch = fix_info.line_length,
           },
       },
    };

    sys->picture = picture_NewFromResource(&vd->fmt, &rsc);
    if (unlikely(sys->picture == NULL)) {
        munmap(rsc.p[0].p_pixels, sys->video_size);
        ioctl(sys->fd, FBIOPUT_VSCREENINFO, &sys->old_info);
        return VLC_ENOMEM;
    }

    ClearScreen(sys);

    msg_Dbg(vd,
            "framebuffer type=%d, visual=%d, ypanstep=%d, ywrap=%d, accel=%d",
            fix_info.type, fix_info.visual,
            fix_info.ypanstep, fix_info.ywrapstep, fix_info.accel);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDisplay: terminate FB video thread output method
 *****************************************************************************/
static void CloseDisplay(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->video_ptr != MAP_FAILED) {
        ClearScreen(sys);
        munmap(sys->video_ptr, sys->video_size);
    }

    if (sys->fd >= 0) {
        /* Restore palette */
        if (sys->var_info.bits_per_pixel == 8) {
            ioctl(sys->fd, FBIOPUTCMAP, &sys->fb_cmap);
            free(sys->palette);
            sys->palette = NULL;
        }

        /* Restore fb config */
        ioctl(sys->fd, FBIOPUT_VSCREENINFO, &sys->old_info);

        /* Close fb */
        vlc_close(sys->fd);
    }
}

#if 0
/*****************************************************************************
 * SwitchDisplay: VT change signal handler
 *****************************************************************************
 * This function activates or deactivates the output of the thread. It is
 * called by the VT driver, on terminal change.
 *****************************************************************************/
static void SwitchDisplay(int i_signal)
{
    vout_display_t *vd;

    vlc_mutex_lock(&p_vout_bank->lock);

    /* XXX: only test the first video output */
    if (p_vout_bank->i_count)
    {
        vd = p_vout_bank->pp_vout[0];

        switch (i_signal)
        {
        case SIGUSR1:                                /* vt has been released */
            vd->b_active = 0;
            ioctl(sys->tty, VT_RELDISP, 1);
            break;
        case SIGUSR2:                                /* vt has been acquired */
            vd->b_active = 1;
            ioctl(sys->tty, VT_RELDISP, VT_ACTIVATE);
            /* handle blanking */
            vlc_mutex_lock(&vd->change_lock);
            vd->i_changes |= VOUT_SIZE_CHANGE;
            vlc_mutex_unlock(&vd->change_lock);
            break;
        }
    }

    vlc_mutex_unlock(&p_vout_bank->lock);
}
#endif

/*****************************************************************************
 * TextMode and GfxMode : switch tty to text/graphic mode
 *****************************************************************************
 * These functions toggle the tty mode.
 *****************************************************************************/
static void TextMode(int tty)
{
    /* return to text mode */
    if (-1 == ioctl(tty, KDSETMODE, KD_TEXT)) {
        /*msg_Err(vd, "failed ioctl KDSETMODE KD_TEXT");*/
    }
}

static void GfxMode(int tty)
{
    /* switch to graphic mode */
    if (-1 == ioctl(tty, KDSETMODE, KD_GRAPHICS)) {
        /*msg_Err(vd, "failed ioctl KDSETMODE KD_GRAPHICS");*/
    }
}
