/*****************************************************************************
 * video.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2004, 2003 VideoLAN
 * $Id$
 *
 * Authors: Marodon Cedric <cedric_marodon@yahoo.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/intf.h>

#include "wince.h"

static void *GetWindow( intf_thread_t *p_intf, vout_thread_t *,
                        int *pi_x_hint, int *pi_y_hint,
                        unsigned int *pi_width_hint,
                        unsigned int *pi_height_hint );
static void ReleaseWindow( intf_thread_t *p_intf, void *p_window );

static int ControlWindow( intf_thread_t *p_intf, void *p_window,
                          int i_query, va_list args );

/* IDs for the controls and the menu commands */
enum
{
    UpdateSize_Event = 1000 + 1,
    UpdateHide_Event
};

/* Video Window */
class VideoWindow : public CBaseWindow
{
public:
    /* Constructor */
    VideoWindow( intf_thread_t *_p_intf, HWND _p_parent );
    virtual ~VideoWindow();

    void *GetWindow( vout_thread_t *, int *, int *, unsigned int *,
                     unsigned int * );
    void ReleaseWindow( void * );
    int  ControlWindow( void *, int, va_list );
        
    HWND p_child_window; // public because of menu

private:
    intf_thread_t *p_intf;
    vout_thread_t *p_vout;
    HWND p_parent;
    vlc_mutex_t lock;

    virtual LRESULT WndProc( HWND, UINT, WPARAM, LPARAM );
};

/*****************************************************************************
 * Public methods.
 *****************************************************************************/
CBaseWindow *CreateVideoWindow( intf_thread_t *p_intf, HWND p_parent )
{
    return new VideoWindow( p_intf, p_parent );
}

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
VideoWindow::VideoWindow( intf_thread_t *_p_intf, HWND _p_parent )
{
    RECT rect;
    WNDCLASS wc;

    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;
    p_child_window = NULL;

    vlc_mutex_init( p_intf, &lock );

    p_vout = NULL;

    p_intf->pf_request_window = ::GetWindow;
    p_intf->pf_release_window = ::ReleaseWindow;
    p_intf->pf_control_window = ::ControlWindow;

    p_intf->p_sys->p_video_window = this;

    GetClientRect( p_parent, &rect );

    wc.style = CS_HREDRAW | CS_VREDRAW ;
    wc.lpfnWndProc = (WNDPROC)BaseWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hIcon = NULL;
    wc.hInstance = GetModuleHandle(0);
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("VIDEOWINDOW");
    RegisterClass( &wc );

    p_child_window = CreateWindow (
        _T("VIDEOWINDOW"), _T(""),
        WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE | WS_BORDER,
        0, 20, rect.right - rect.left,
        rect.bottom - rect.top - 2*(MENU_HEIGHT-1) - SLIDER_HEIGHT - 20,
        p_parent, NULL, GetModuleHandle(0), (void *)this );

    ShowWindow( p_child_window, SW_SHOW );
    UpdateWindow( p_child_window );

    ReleaseWindow( (void*)p_child_window );
}

VideoWindow::~VideoWindow()
{
    vlc_mutex_destroy( &lock );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
static void *GetWindow( intf_thread_t *p_intf,  vout_thread_t *p_vout,
                        int *pi_x_hint, int *pi_y_hint,
                        unsigned int *pi_width_hint,
                        unsigned int *pi_height_hint )
{
    return p_intf->p_sys->p_video_window->GetWindow( p_vout, pi_x_hint,
                                    pi_y_hint, pi_width_hint, pi_height_hint );
}

void *VideoWindow::GetWindow( vout_thread_t *_p_vout,
                              int *pi_x_hint, int *pi_y_hint,
                              unsigned int *pi_width_hint,
                              unsigned int *pi_height_hint )
{
    vlc_mutex_lock( &lock );

    if( p_vout )
    {
        vlc_mutex_unlock( &lock );
        msg_Dbg( p_intf, "Video window already in use" );
        return NULL;
    }

    p_vout = _p_vout;

    vlc_mutex_unlock( &lock );

    ShowWindow( (HWND)p_child_window, SW_SHOW );
    return p_child_window;
}

static void ReleaseWindow( intf_thread_t *p_intf, void *p_window )
{
    p_intf->p_sys->p_video_window->ReleaseWindow( p_window );
}

void VideoWindow::ReleaseWindow( void *p_window )
{
    vlc_mutex_lock( &lock );
    p_vout = NULL;
    vlc_mutex_unlock( &lock );

    ShowWindow( (HWND)p_window, SW_HIDE );
    SetForegroundWindow( p_parent );
}

/***********************************************************************

FUNCTION:
  WndProc

PURPOSE: 
  Processes messages sent to the main window.

***********************************************************************/
LRESULT VideoWindow::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch( msg )
    {
    case WM_KILLFOCUS:
        if( p_vout )
            vout_Control( p_vout, VOUT_SET_FOCUS, (vlc_bool_t)VLC_FALSE );
        return TRUE;

    case WM_SETFOCUS:
        if( p_vout )
            vout_Control( p_vout, VOUT_SET_FOCUS, (vlc_bool_t)VLC_TRUE );
        return TRUE;

    default:
        return DefWindowProc( hwnd, msg, wp, lp );
    }

}

static int ControlWindow( intf_thread_t *p_intf, void *p_window,
                          int i_query, va_list args )
{
    return p_intf->p_sys->p_video_window->ControlWindow( p_window, i_query,
                                                         args );
}

int VideoWindow::ControlWindow( void *p_window, int i_query, va_list args )
{
    int i_ret = VLC_EGENERIC;

    vlc_mutex_lock( &lock );

    switch( i_query )
    {
    case VOUT_SET_ZOOM:
        break;

    case VOUT_SET_STAY_ON_TOP:
        break;

    default:
        msg_Dbg( p_intf, "control query not supported" );
        break;
    }

    vlc_mutex_unlock( &lock );

    return i_ret;
}
