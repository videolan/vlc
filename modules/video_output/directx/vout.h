/*****************************************************************************
 * vout.h: Windows DirectX video output header file
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vout.h,v 1.4 2003/03/28 17:02:25 gbazin Exp $
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
 * event_thread_t: DirectX event thread
 *****************************************************************************/
typedef struct event_thread_t
{
    VLC_COMMON_MEMBERS

    vout_thread_t * p_vout;

} event_thread_t;

/*****************************************************************************
 * vout_sys_t: video output DirectX method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the DirectX specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    LPDIRECTDRAW2        p_ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE2 p_display;                        /* Display device */
    LPDIRECTDRAWSURFACE2 p_current_surface;   /* surface currently displayed */
    LPDIRECTDRAWCLIPPER  p_clipper;             /* clipper used for blitting */
    HINSTANCE            hddraw_dll;       /* handle of the opened ddraw dll */
    HBRUSH               hbrush;           /* window backgound brush (color) */

    HWND                 hwnd;                  /* Handle of the main window */
    HWND                 hparent;             /* Handle of the parent window */
    WNDPROC              pf_wndproc;             /* Window handling callback */

    vlc_bool_t   b_using_overlay;         /* Are we using an overlay surface */
    vlc_bool_t   b_use_sysmem;   /* Should we use system memory for surfaces */
    vlc_bool_t   b_hw_yuv;    /* Should we use hardware YUV->RGB conversions */
    vlc_bool_t   b_3buf_overlay;   /* Should we use triple buffered overlays */

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

    event_thread_t *    p_event;
};

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * DirectX specific properties of a direct buffer.
 *****************************************************************************/
struct picture_sys_t
{
    LPDIRECTDRAWSURFACE2 p_surface;
    DDSURFACEDESC        ddsd;
    LPDIRECTDRAWSURFACE2 p_front_surface;
};

/*****************************************************************************
 * Prototypes from vout.c
 *****************************************************************************/
void DirectXUpdateOverlay( vout_thread_t *p_vout );

/*****************************************************************************
 * Prototypes from events.c
 *****************************************************************************/
void DirectXEventThread ( event_thread_t *p_event );
void DirectXUpdateRects ( vout_thread_t *p_vout, vlc_bool_t b_force );

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define WM_VLC_HIDE_MOUSE WM_APP
#define IDM_TOGGLE_ON_TOP WM_USER + 1
