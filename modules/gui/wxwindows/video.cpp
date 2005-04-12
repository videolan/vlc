/*****************************************************************************
 * video.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004, 2003 VideoLAN
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

#include "wxwindows.h"

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

class VideoWindow: public wxWindow
{
public:
    /* Constructor */
    VideoWindow( intf_thread_t *_p_intf, wxWindow *p_parent );
    virtual ~VideoWindow();

    void *GetWindow( vout_thread_t *p_vout, int *, int *,
                     unsigned int *, unsigned int * );
    void ReleaseWindow( void * );
    int  ControlWindow( void *, int, va_list );

private:
    intf_thread_t *p_intf;
    vout_thread_t *p_vout;
    wxWindow *p_parent;
    vlc_mutex_t lock;
    vlc_bool_t b_shown;

    wxWindow *p_child_window;

    wxTimer m_hide_timer;

    void UpdateSize( wxEvent& event );
    void UpdateHide( wxEvent& event );
    void OnControlEvent( wxCommandEvent& event );
    void OnHideTimer( wxTimerEvent& WXUNUSED(event));

    DECLARE_EVENT_TABLE();
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

    int size_to_video = config_GetInt( p_intf, "wxwin-size-to-video" );

    p_vout = NULL;

    m_hide_timer.SetOwner(this, ID_HIDE_TIMER);

    p_intf->pf_request_window = ::GetWindow;
    p_intf->pf_release_window = ::ReleaseWindow;
    p_intf->pf_control_window = ::ControlWindow;

    p_intf->p_sys->p_video_window = this;
    
    wxSize child_size = wxSize(0,0);
    if (!size_to_video)
    {
      //maybe this size should be an option
      child_size = wxSize( wxSystemSettings::GetMetric( wxSYS_SCREEN_X ) / 2,
                           wxSystemSettings::GetMetric( wxSYS_SCREEN_Y ) / 2);

      SetSize(child_size);
    }

    p_child_window = new wxWindow( this, -1, wxDefaultPosition, child_size );
    
    
    if (!size_to_video)
    {
      //show the window so we can see the background where the video will be
    p_child_window->Show();
    Show();
    b_shown = VLC_TRUE;
    }
    else
    {
      //othewise the video window is shown when a video starts
      //but hide it now, so we can set the background color without it showing
      p_child_window->Hide();
      Hide();
      b_shown = VLC_FALSE;
    }

    p_child_window->SetBackgroundColour(*wxBLACK);
    SetBackgroundColour(*wxBLACK);

    p_intf->p_sys->p_video_sizer = new wxBoxSizer( wxHORIZONTAL );
    p_intf->p_sys->p_video_sizer->Add( this, 1, wxEXPAND );

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

    int size_to_video = config_GetInt( p_intf, "wxwin-size-to-video" );

    if (size_to_video)
    {
#if defined(__WXGTK__) || defined(WIN32)
    wxSizeEvent event( wxSize(0, 0), UpdateHide_Event );
    AddPendingEvent( event );
#endif
    }

    vlc_mutex_unlock( &lock );
}

void VideoWindow::UpdateSize( wxEvent &_event )
{
  int size_to_video = config_GetInt( p_intf, "wxwin-size-to-video" );

  m_hide_timer.Stop();

  if (size_to_video)
  {
    wxSizeEvent * event = (wxSizeEvent*)(&_event);
    if( !b_shown )
    {
        p_intf->p_sys->p_video_sizer->Show( this, TRUE );
        p_intf->p_sys->p_video_sizer->Layout();
        SetFocus();
        b_shown = VLC_TRUE;
    }
    p_intf->p_sys->p_video_sizer->SetMinSize( event->GetSize() );

    wxCommandEvent intf_event( wxEVT_INTF, 0 );
    p_parent->AddPendingEvent( intf_event );
}
  else
  {
    //this is a very hackish way to show in case the user switched
    //size-to-video off while no video was playing, which would leave
    //the window hidden and of 0 size
    if( !b_shown )
    {
        p_intf->p_sys->p_video_sizer->Show( this, TRUE );
        SetFocus();
        b_shown = VLC_TRUE;

        wxSize child_size = wxSize( wxSystemSettings::GetMetric( wxSYS_SCREEN_X ) / 2,
                                    wxSystemSettings::GetMetric( wxSYS_SCREEN_Y ) / 2);

        SetSize(child_size);
        p_child_window->SetSize(child_size);

        Interface * intf = (Interface *)p_parent;
        intf->frame_sizer->Layout();
        intf->frame_sizer->Fit(intf);
    }
  }
}

void VideoWindow::UpdateHide( wxEvent &_event )
{
  int size_to_video = config_GetInt( p_intf, "wxwin-size-to-video" );

  if (size_to_video)
  {
    m_hide_timer.Start(1000, wxTIMER_ONE_SHOT);
  }
}

void VideoWindow::OnHideTimer( wxTimerEvent& WXUNUSED(event))
{
  //wxSizeEvent * event = (wxSizeEvent*)(&_event);
    if( b_shown )
    {
        p_intf->p_sys->p_video_sizer->Show( this, FALSE );
        p_intf->p_sys->p_video_sizer->Layout();
        b_shown = VLC_FALSE;

        SetSize(0,0);
      Hide();
    }
  //ok I cheat here, but is it ever not 0,0?
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
        case VOUT_SET_ZOOM:
        {
            int size_to_video = config_GetInt( p_intf, "wxwin-size-to-video" );

            if (size_to_video)
            {
            double f_arg = va_arg( args, double );

            /* Update dimensions */
            wxSizeEvent event( wxSize((int)(p_vout->i_window_width * f_arg),
                                      (int)(p_vout->i_window_height * f_arg)),
                               UpdateSize_Event );

              
            AddPendingEvent( event );
            }

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
