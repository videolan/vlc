/*****************************************************************************
 * vout_directx.h: Windows DirectX video output header file
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vout_directx.h,v 1.1 2001/07/11 14:26:19 gbazin Exp $
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
typedef struct vout_sys_s
{

    LPDIRECTDRAW2        p_ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE3 p_display;                        /* Display device */
    LPDIRECTDRAWSURFACE3 p_surface;    /* surface where we display the video */
    LPDIRECTDRAWCLIPPER  p_clipper;             /* clipper used for blitting */
    HINSTANCE            hddraw_dll;       /* handle of the opened ddraw dll */
    HBRUSH               hbrush;           /* window backgound brush (color) */
    HWND                 hwnd;                  /* Handle of the main window */

    int         i_image_width;                  /* size of the decoded image */
    int         i_image_height;
    int         i_window_width;               /* size of the displayed image */
    int         i_window_height;

    int         i_colorkey;          /* colorkey used to display the overlay */
 
    boolean_t   b_display_enabled;
    boolean_t   b_cursor;

    u16         i_changes;              /* changes made to the video display */

    boolean_t   b_cursor_autohidden;
    mtime_t     i_lastmoved;

    char       *p_directx_buf[2];                      /* Buffer information */

    vlc_thread_t event_thread_id;                            /* event thread */
    vlc_mutex_t  event_thread_lock;             /* lock for the event thread */
    vlc_cond_t   event_thread_wait;

    int          i_event_thread_status;         /* DirectXEventThread status */
    boolean_t    b_event_thread_die;        /* flag to kill the event thread */

} vout_sys_t;

/*****************************************************************************
 * Prototypes from vout_directx.c
 *****************************************************************************/

/*****************************************************************************
 * Prototypes from vout_events.c
 *****************************************************************************/
void DirectXEventThread ( vout_thread_t *p_vout );
