/*****************************************************************************
 * vout_xvideo.c: Xvideo video output display method
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vout_common.h,v 1.2 2001/12/13 12:47:17 sam Exp $
 *
 * Authors: Shane Harper <shanegh@optusnet.com.au>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
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
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 and XVideo specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* User settings */
#if 0
    /* this plugin (currently) requires the SHM Ext... */
    boolean_t           b_shm;               /* shared memory extension flag */
#endif

    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */
#if MODULE_NAME == xvideo
    Visual *            p_visual;                          /* visual pointer */
#endif
    int                 i_screen;                           /* screen number */
    Window              window;                               /* root window */
    GC                  gc;              /* graphic context instance handler */
#if MODULE_NAME == xvideo
    Window              yuv_window;   /* sub-window for displaying yuv video
                                                                        data */
    GC                  yuv_gc;
    int                 i_xvport;
#else
    Colormap            colormap;               /* colormap used (8bpp only) */

    /* Display buffers and shared memory information */
    XImage *            p_ximage[2];                       /* XImage pointer */
    XShmSegmentInfo     shm_info[2];       /* shared memory zone information */
#endif

    /* X11 generic properties */
    Atom                wm_protocols;
    Atom                wm_delete_window;

    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */

    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */

    /* Mouse pointer properties */
    boolean_t           b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    Cursor              blank_cursor;                   /* the hidden cursor */
    Pixmap              cursor_pixmap;

} vout_sys_t;

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * XVideo specific properties of a direct buffer.
 *****************************************************************************/
typedef struct picture_sys_s
{
    XvImage *           p_xvimage;
    XShmSegmentInfo     shminfo;       /* shared memory zone information */

} picture_sys_t;

/*****************************************************************************
 * mwmhints_t: window manager hints
 *****************************************************************************
 * Fullscreen needs to be able to hide the wm decorations so we provide
 * this structure to make it easier.
 *****************************************************************************/
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct mwmhints_s
{
    u32 flags;
    u32 functions;
    u32 decorations;
    s32 input_mode;
    u32 status;
} mwmhints_t;

/*****************************************************************************
 * Common prototypes
 *****************************************************************************/
int  _M( vout_Manage )   ( struct vout_thread_s * );

int  _M( XCommonCreateWindow )    ( vout_thread_t *p_vout );
void _M( XCommonDestroyWindow )   ( vout_thread_t *p_vout );

void _M( XCommonEnableScreenSaver )       ( vout_thread_t *p_vout );
void _M( XCommonDisableScreenSaver )      ( vout_thread_t *p_vout );
void _M( XCommonToggleMousePointer )      ( vout_thread_t *p_vout );

