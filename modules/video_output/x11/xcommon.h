/*****************************************************************************
 * xcommon.h: Defines common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: xcommon.h,v 1.1 2002/08/04 17:23:44 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Defines
 *****************************************************************************/
#ifdef MODULE_NAME_IS_xvideo
#   define IMAGE_TYPE     XvImage
#   define EXTRA_ARGS     int i_xvport, int i_chroma
#   define EXTRA_ARGS_SHM int i_xvport, int i_chroma, XShmSegmentInfo *p_shm
#   define DATA_SIZE(p)   (p)->data_size
#   define IMAGE_FREE     XFree      /* There is nothing like XvDestroyImage */
#else
#   define IMAGE_TYPE     XImage
#   define EXTRA_ARGS     Visual *p_visual, int i_depth, int i_bytes_per_pixel
#   define EXTRA_ARGS_SHM Visual *p_visual, int i_depth, XShmSegmentInfo *p_shm
#   define DATA_SIZE(p)   ((p)->bytes_per_line * (p)->height)
#   define IMAGE_FREE     XDestroyImage
#endif

VLC_DECLARE_STRUCT(x11_window_t)

/*****************************************************************************
 * x11_window_t: X11 window descriptor
 *****************************************************************************
 * This structure contains all the data necessary to describe an X11 window.
 *****************************************************************************/
struct x11_window_t
{
    Window              base_window;                          /* base window */
    Window              video_window;     /* sub-window for displaying video */
    GC                  gc;              /* graphic context instance handler */
    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */
    Atom                wm_protocols;
    Atom                wm_delete_window;
};

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 and XVideo specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */

    Visual *            p_visual;                          /* visual pointer */
    int                 i_screen;                           /* screen number */

    /* Our current window */
    x11_window_t *      p_win;

    /* Our two windows */
    x11_window_t        original_window;
    x11_window_t        fullscreen_window;

    /* X11 generic properties */
    vlc_bool_t          b_altfullscreen;          /* which fullscreen method */
    vlc_bool_t          b_createwindow;  /* are we the base window's owner ? */
#ifdef HAVE_SYS_SHM_H
    vlc_bool_t          b_shm;               /* shared memory extension flag */
#endif

#ifdef MODULE_NAME_IS_xvideo
    int                 i_xvport;
#else
    Colormap            colormap;               /* colormap used (8bpp only) */

    int                 i_screen_depth;
    int                 i_bytes_per_pixel;
    int                 i_bytes_per_line;
#endif

    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */
#ifdef DPMSINFO_IN_DPMS_H
    BOOL                b_ss_dpms;                              /* DPMS mode */
#endif

    /* Mouse pointer properties */
    vlc_bool_t          b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    Cursor              blank_cursor;                   /* the hidden cursor */
    mtime_t             i_time_button_last_pressed;   /* to track dbl-clicks */
    Pixmap              cursor_pixmap;
};

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * XVideo specific properties of a direct buffer.
 *****************************************************************************/
struct picture_sys_t
{
    IMAGE_TYPE *        p_image;

#ifdef HAVE_SYS_SHM_H
    XShmSegmentInfo     shminfo;       /* shared memory zone information */
#endif
};

/*****************************************************************************
 * mwmhints_t: window manager hints
 *****************************************************************************
 * Fullscreen needs to be able to hide the wm decorations so we provide
 * this structure to make it easier.
 *****************************************************************************/
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct mwmhints_t
{
    u32 flags;
    u32 functions;
    u32 decorations;
    s32 input_mode;
    u32 status;
} mwmhints_t;

/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#ifdef MODULE_NAME_IS_xvideo
#   define MAX_DIRECTBUFFERS 10
#else
#   define MAX_DIRECTBUFFERS 2
#endif

