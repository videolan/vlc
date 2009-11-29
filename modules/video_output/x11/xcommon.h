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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Defines
 *****************************************************************************/
struct vout_window_t;

/*****************************************************************************
 * x11_window_t: X11 window descriptor
 *****************************************************************************
 * This structure contains all the data necessary to describe an X11 window.
 *****************************************************************************/
typedef struct x11_window_t
{
    struct vout_window_t*owner_window;               /* owner window (if any) */
    Window              base_window;                          /* base window */
    Window              video_window;     /* sub-window for displaying video */
    GC                  gc;              /* graphic context instance handler */

    unsigned int        i_width;                             /* window width */
    unsigned int        i_height;                           /* window height */
    int                 i_x;                          /* window x coordinate */
    int                 i_y;                          /* window y coordinate */

    Atom                wm_protocols;
    Atom                wm_delete_window;
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

    int                 i_screen;                           /* screen number */

    /* Our window */
    x11_window_t        window;

    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */
#ifdef DPMSINFO_IN_DPMS_H
    BOOL                b_ss_dpms;                              /* DPMS mode */
#endif

    /* Mouse pointer properties */
    bool          b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    mtime_t             i_mouse_hide_timeout;      /* after time hide cursor */
    Cursor              blank_cursor;                   /* the hidden cursor */
    mtime_t             i_time_button_last_pressed;   /* to track dbl-clicks */
    Pixmap              cursor_pixmap;

#ifdef MODULE_NAME_IS_glx
    /* GLX properties */
    int                 b_glx13;
    GLXContext          gwctx;
    GLXWindow           gwnd;
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
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    signed   long input_mode;
    unsigned long status;
} mwmhints_t;

