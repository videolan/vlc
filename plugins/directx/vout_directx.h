/*****************************************************************************
 * vout_directx.h: Windows DirectX video output header file
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vout_directx.h,v 1.7 2002/06/01 12:31:58 sam Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
 * vout_sys_t: video output DirectX method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the DirectX specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_s
{

    LPDIRECTDRAW2        p_ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE3 p_display;                        /* Display device */
    LPDIRECTDRAWSURFACE3 p_current_surface;   /* surface currently displayed */
    LPDIRECTDRAWCLIPPER  p_clipper;             /* clipper used for blitting */
    HINSTANCE            hddraw_dll;       /* handle of the opened ddraw dll */
    HBRUSH               hbrush;           /* window backgound brush (color) */
    HWND                 hwnd;                  /* Handle of the main window */

    vlc_bool_t   b_using_overlay;         /* Are we using an overlay surface */
    vlc_bool_t   b_use_sysmem;   /* Should we use system memory for surfaces */
    vlc_bool_t   b_hw_yuv;    /* Should we use hardware YUV->RGB conversions */

    /* size of the display */
    RECT         rect_display;
    int          i_display_depth;

    /* Window position and size */
    int          i_window_x;
    int          i_window_y;
    int          i_window_width;
    int          i_window_height;

    /* Coordinates of src and dest images (used when blitting to display) */
    RECT         rect_src;
    RECT         rect_src_clipped;
    RECT         rect_dest;
    RECT         rect_dest_clipped;

    /* DDraw capabilities */
    int          b_caps_overlay_clipping;

    int          i_rgb_colorkey;      /* colorkey in RGB used by the overlay */
    int          i_colorkey;                 /* colorkey used by the overlay */
 
    volatile u16 i_changes;             /* changes made to the video display */

    /* Mouse */
    volatile vlc_bool_t b_cursor_hidden;
    volatile mtime_t    i_lastmoved;

    vlc_thread_t event_thread_id;                            /* event thread */
    vlc_mutex_t  event_thread_lock;             /* lock for the event thread */
    vlc_cond_t   event_thread_wait;

    volatile int i_event_thread_status;         /* DirectXEventThread status */
    volatile vlc_bool_t b_event_thread_die; /* flag to kill the event thread */
};

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * DirectX specific properties of a direct buffer.
 *****************************************************************************/
struct picture_sys_s
{
    LPDIRECTDRAWSURFACE3 p_surface;
    DDSURFACEDESC        ddsd;
    LPDIRECTDRAWSURFACE3 p_front_surface;
};

/*****************************************************************************
 * Prototypes from vout_directx.c
 *****************************************************************************/

/*****************************************************************************
 * Prototypes from vout_events.c
 *****************************************************************************/
void DirectXEventThread ( vout_thread_t *p_vout );
void DirectXUpdateOverlay( vout_thread_t *p_vout );

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define WM_VLC_HIDE_MOUSE WM_APP
