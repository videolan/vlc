/*****************************************************************************
 * video.cpp : wxWidgets plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004, 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include "video.hpp"
#include "interface.hpp"

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
    UpdateSize_Event = wxID_HIGHEST + 1,
    UpdateHide_Event,
    SetStayOnTop_Event,
    ID_HIDE_TIMER
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_VLC_VIDEO );

BEGIN_EVENT_TABLE(VideoWindow, wxWindow)
    EVT_CUSTOM( wxEVT_SIZE, UpdateSize_Event, VideoWindow::UpdateSize )
    EVT_CUSTOM( wxEVT_SIZE, UpdateHide_Event, VideoWindow::UpdateHide )
    EVT_COMMAND( SetStayOnTop_Event, wxEVT_VLC_VIDEO,
                 VideoWindow::OnControlEvent )
    EVT_TIMER( ID_HIDE_TIMER, VideoWindow::OnHideTimer )
END_EVENT_TABLE()

/*****************************************************************************
 * Public methods.
 *****************************************************************************/
wxWindow *CreateVideoWindow( intf_thread_t *p_intf, wxWindow *p_parent )
{
    return new VideoWindow( p_intf, p_parent );
}

void UpdateVideoWindow( intf_thread_t *p_intf, wxWindow *p_window )
{
#if (wxCHECK_VERSION(2,5,0))
    if( p_window && mdate() - ((VideoWindow *)p_window)->i_creation_date < 2000000 )
        return; /* Hack to prevent saving coordinates if window is not yet
                 * properly created. Yuck :( */

    if( p_window && p_intf->p_sys->p_video_sizer && p_window->IsShown() )
        p_intf->p_sys->p_video_sizer->SetMinSize( p_window->GetSize() );
#endif
}

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
VideoWindow::VideoWindow( intf_thread_t *_p_intf, wxWindow *_p_parent ):
    wxWindow( _p_parent, -1 )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;

    vlc_mutex_init( p_intf, &lock );

    b_auto_size = config_GetInt( p_intf, "wx-autosize" );

    p_vout = NULL;
    i_creation_date = 0;
    m_hide_timer.SetOwner( this, ID_HIDE_TIMER );

    p_intf->pf_request_window = ::GetWindow;
    p_intf->pf_release_window = ::ReleaseWindow;
    p_intf->pf_control_window = ::ControlWindow;

    p_intf->p_sys->p_video_window = this;

    wxSize child_size = wxSize(0,0);
    if( !b_auto_size )
    {
        WindowSettings *ws = p_intf->p_sys->p_window_settings;
        wxPoint p; bool b_shown;

        // Maybe this size should be an option
        child_size = wxSize( wxSystemSettings::GetMetric(wxSYS_SCREEN_X) / 2,
                             wxSystemSettings::GetMetric(wxSYS_SCREEN_Y) / 2 );

        ws->GetSettings( WindowSettings::ID_VIDEO, b_shown, p, child_size );
        SetSize( child_size );
    }

    p_child_window = new wxWindow( this, -1, wxDefaultPosition, child_size );

    if( !b_auto_size )
    {
        SetBackgroundColour( *wxBLACK );
        p_child_window->SetBackgroundColour( *wxBLACK );
    }

    p_child_window->Show();
    Show();
    b_shown = VLC_TRUE;

    p_intf->p_sys->p_video_sizer = new wxBoxSizer( wxHORIZONTAL );
#if (wxCHECK_VERSION(2,5,3))
    p_intf->p_sys->p_video_sizer->Add( this, 1, wxEXPAND|wxFIXED_MINSIZE );
#else
    p_intf->p_sys->p_video_sizer->Add( this, 1, wxEXPAND );
#endif

    ReleaseWindow( NULL );
}

VideoWindow::~VideoWindow()
{
    vlc_mutex_lock( &lock );
    if( p_vout )
    {
        if( !p_intf->psz_switch_intf )
        {
            if( vout_Control( p_vout, VOUT_CLOSE ) != VLC_SUCCESS )
                vout_Control( p_vout, VOUT_REPARENT );
        }
        else
        {
            if( vout_Control( p_vout, VOUT_REPARENT ) != VLC_SUCCESS )
                vout_Control( p_vout, VOUT_CLOSE );
        }
    }

    p_intf->pf_request_window = NULL;
    p_intf->pf_release_window = NULL;
    p_intf->pf_control_window = NULL;
    vlc_mutex_unlock( &lock );

    if( !b_auto_size )
    {
        WindowSettings *ws = p_intf->p_sys->p_window_settings;
        ws->SetSettings( WindowSettings::ID_VIDEO, true,
                         GetPosition(), GetSize() );
    }

    vlc_mutex_destroy( &lock );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
static void *GetWindow( intf_thread_t *p_intf, vout_thread_t *p_vout,
                        int *pi_x_hint, int *pi_y_hint,
                        unsigned int *pi_width_hint,
                        unsigned int *pi_height_hint )
{
    return p_intf->p_sys->p_video_window->GetWindow( p_vout,
                                                     pi_x_hint, pi_y_hint,
                                                     pi_width_hint,
                                                     pi_height_hint );
}

/* Part of the hack to get the X11 window handle from the GtkWidget */
#ifdef __WXGTK__
extern "C" {
#ifdef __WXGTK20__
    int gdk_x11_drawable_get_xid( void * );
#endif
    void *gtk_widget_get_parent_window( void * );
}
#endif

void *VideoWindow::GetWindow( vout_thread_t *_p_vout,
                              int *pi_x_hint, int *pi_y_hint,
                              unsigned int *pi_width_hint,
                              unsigned int *pi_height_hint )
{
#if defined(__WXGTK__) || defined(WIN32)
    vlc_mutex_lock( &lock );

    if( p_vout )
    {
        vlc_mutex_unlock( &lock );
        msg_Dbg( p_intf, "Video window already in use" );
        return NULL;
    }

    p_vout = _p_vout;

    wxSizeEvent event( wxSize(*pi_width_hint, *pi_height_hint),
                       UpdateSize_Event );
    AddPendingEvent( event );
    vlc_mutex_unlock( &lock );

#ifdef __WXGTK__
    GtkWidget *p_widget = p_child_window->GetHandle();

#ifdef __WXGTK20__
    return (void *)gdk_x11_drawable_get_xid(
               gtk_widget_get_parent_window( p_widget ) );
#elif defined(__WXGTK__)
    return (void *)*(int *)( (char *)gtk_widget_get_parent_window( p_widget )
               + 2 * sizeof(void *) );
#endif

#elif defined(WIN32)
    return (void*)GetHandle();

#endif

#else // defined(__WXGTK__) || defined(WIN32)
    return NULL;

#endif
}

static void ReleaseWindow( intf_thread_t *p_intf, void *p_window )
{
    return p_intf->p_sys->p_video_window->ReleaseWindow( p_window );
}

void VideoWindow::ReleaseWindow( void *p_window )
{
    vlc_mutex_lock( &lock );
    p_vout = NULL;
    vlc_mutex_unlock( &lock );

    if( !b_auto_size ) return;

#if defined(__WXGTK__) || defined(WIN32)
    wxSizeEvent event( wxSize(0, 0), UpdateHide_Event );
    AddPendingEvent( event );
#endif
}

void VideoWindow::UpdateSize( wxEvent &_event )
{
    m_hide_timer.Stop();

    if( !b_auto_size ) return;

    wxSizeEvent * event = (wxSizeEvent*)(&_event);
    if( !b_shown )
    {
        p_intf->p_sys->p_video_sizer->Show( this, TRUE );
        p_intf->p_sys->p_video_sizer->Layout();
        SetFocus();
        b_shown = VLC_TRUE;
    }

    p_intf->p_sys->p_video_sizer->SetMinSize( event->GetSize() );

    i_creation_date = mdate();
    wxCommandEvent intf_event( wxEVT_INTF, 0 );
    p_parent->AddPendingEvent( intf_event );
}

void VideoWindow::UpdateHide( wxEvent &_event )
{
    if( b_auto_size ) m_hide_timer.Start( 200, wxTIMER_ONE_SHOT );
}

void VideoWindow::OnHideTimer( wxTimerEvent& WXUNUSED(event))
{
    if( b_shown )
    {
        p_intf->p_sys->p_video_sizer->Show( this, FALSE );
        SetSize( 0, 0 );
        p_intf->p_sys->p_video_sizer->Layout();
        b_shown = VLC_FALSE;
    }
    p_intf->p_sys->p_video_sizer->SetMinSize( wxSize(0,0) );

    wxCommandEvent intf_event( wxEVT_INTF, 0 );
    p_parent->AddPendingEvent( intf_event );
}

void VideoWindow::OnControlEvent( wxCommandEvent &event )
{
    switch( event.GetId() )
    {
    case SetStayOnTop_Event:
        wxCommandEvent intf_event( wxEVT_INTF, 1 );
        intf_event.SetInt( event.GetInt() );
        p_parent->AddPendingEvent( intf_event );
        break;
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
        case VOUT_GET_SIZE:
        {
            unsigned int *pi_width  = va_arg( args, unsigned int * );
            unsigned int *pi_height = va_arg( args, unsigned int * );

	    *pi_width = GetSize().GetWidth();
	    *pi_height = GetSize().GetHeight();
            i_ret = VLC_SUCCESS;
        }
        break;

        case VOUT_SET_SIZE:
        {
            if( !b_auto_size ) break;

            unsigned int i_width  = va_arg( args, unsigned int );
            unsigned int i_height = va_arg( args, unsigned int );

            vlc_mutex_lock( &lock );
            if( !i_width && p_vout ) i_width = p_vout->i_window_width;
            if( !i_height && p_vout ) i_height = p_vout->i_window_height;
            vlc_mutex_unlock( &lock );

            /* Update dimensions */
            wxSizeEvent event( wxSize( i_width, i_height ), UpdateSize_Event );

            AddPendingEvent( event );

            i_ret = VLC_SUCCESS;
        }
        break;

        case VOUT_SET_STAY_ON_TOP:
        {
            int i_arg = va_arg( args, int );
            wxCommandEvent event( wxEVT_VLC_VIDEO, SetStayOnTop_Event );
            event.SetInt( i_arg );
            AddPendingEvent( event );

            i_ret = VLC_SUCCESS;
        }
        break;

        default:
            msg_Dbg( p_intf, "control query not supported" );
            break;
    }

    vlc_mutex_unlock( &lock );

    return i_ret;
}
