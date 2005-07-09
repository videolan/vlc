/*****************************************************************************
 * xcommon.h: Defines common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
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
#   define EXTRA_ARGS     int i_xvport, int i_chroma, int i_bits_per_pixel
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

#define X11_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )
#define VLC2X11_FOURCC( i ) \
        X11_FOURCC( ((char *)&i)[0], ((char *)&i)[1], ((char *)&i)[2], \
                    ((char *)&i)[3] )
#define X112VLC_FOURCC( i ) \
        VLC_FOURCC( i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, \
                    (i >> 24) & 0xff )

/*****************************************************************************
 * x11_window_t: X11 window descriptor
 *****************************************************************************
 * This structure contains all the data necessary to describe an X11 window.
 *****************************************************************************/
typedef struct x11_window_t
{
    Window              owner_window;               /* owner window (if any) */
    Window              base_window;                          /* base window */
    Window              video_window;     /* sub-window for displaying video */
    GC                  gc;              /* graphic context instance handler */

    unsigned int        i_width;                             /* window width */
    unsigned int        i_height;                           /* window height */
    int                 i_x;                          /* window x coordinate */
    int                 i_y;                          /* window y coordinate */

    Atom                wm_protocols;
    Atom                wm_delete_window;

#ifdef HAVE_XINERAMA
    int                 i_screen;
#endif

} x11_window_t;

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

    vlc_mutex_t         lock;

    /* Our current window */
    x11_window_t *      p_win;

    /* Our two windows */
    x11_window_t        original_window;
    x11_window_t        fullscreen_window;

    /* X11 generic properties */
    vlc_bool_t          b_altfullscreen;          /* which fullscreen method */
#ifdef HAVE_SYS_SHM_H
    vlc_bool_t          b_shm;               /* shared memory extension flag */
#endif

#ifdef MODULE_NAME_IS_xvideo
    int                 i_xvport;
#else
    Colormap            colormap;               /* colormap used (8bpp only) */

    unsigned int        i_screen_depth;
    unsigned int        i_bytes_per_pixel;
    unsigned int        i_bytes_per_line;
#endif

    /* Screen saver properties */
    unsigned int        i_ss_timeout;                             /* timeout */
    unsigned int        i_ss_interval;           /* interval between changes */
    unsigned int        i_ss_blanking;                      /* blanking mode */
    unsigned int        i_ss_exposure;                      /* exposure mode */
#ifdef DPMSINFO_IN_DPMS_H
    BOOL                b_ss_dpms;                              /* DPMS mode */
#endif

    /* Mouse pointer properties */
    vlc_bool_t          b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    Cursor              blank_cursor;                   /* the hidden cursor */
    mtime_t             i_time_button_last_pressed;   /* to track dbl-clicks */
    Pixmap              cursor_pixmap;

    /* Window manager properties */
    Atom                net_wm_state;
    Atom                net_wm_state_fullscreen;
    vlc_bool_t          b_net_wm_state_fullscreen;
    Atom                net_wm_state_above;
    vlc_bool_t          b_net_wm_state_above;
    Atom                net_wm_state_stays_on_top;
    vlc_bool_t          b_net_wm_state_stays_on_top;
    Atom                net_wm_state_below;
    vlc_bool_t          b_net_wm_state_below;

#ifdef MODULE_NAME_IS_glx
    /* GLX properties */
    int                 b_glx13;
    GLXContext          gwctx;
    GLXWindow           gwnd;
#endif
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
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t  input_mode;
    uint32_t status;

} mwmhints_t;

/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#ifdef MODULE_NAME_IS_xvideo
#   define MAX_DIRECTBUFFERS 10
#else
#   define MAX_DIRECTBUFFERS 2
#endif

