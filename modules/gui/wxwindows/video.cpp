/*****************************************************************************
 * video.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004, 2003 VideoLAN
 * $Id: interface.cpp 6961 2004-03-05 17:34:23Z sam $
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
#include "stream_control.h"

#include "wxwindows.h"

static void *GetWindow( intf_thread_t *p_intf, int *pi_x_hint, int *pi_y_hint,
                        unsigned int *pi_width_hint,
                        unsigned int *pi_height_hint );
static void ReleaseWindow( intf_thread_t *p_intf, void *p_window );

/* IDs for the controls and the menu commands */
enum
{
    UpdateSize_Event = wxID_HIGHEST + 1,
    UpdateHide_Event
};

class VideoWindow: public wxWindow
{
public:
    /* Constructor */
    VideoWindow( intf_thread_t *_p_intf, wxWindow *p_parent );
    virtual ~VideoWindow();

    void *GetWindow( int *, int *, unsigned int *, unsigned int * );
    void ReleaseWindow( void * );

private:
    intf_thread_t *p_intf;
    wxWindow *p_parent;
    vlc_mutex_t lock;
    vlc_bool_t  b_in_use;

    wxWindow *p_child_window;

    void UpdateSize( wxSizeEvent & );
    void UpdateHide( wxSizeEvent & );

    DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(VideoWindow, wxWindow)
    EVT_CUSTOM( wxEVT_SIZE, UpdateSize_Event, VideoWindow::UpdateSize )
    EVT_CUSTOM( wxEVT_SIZE, UpdateHide_Event, VideoWindow::UpdateHide )
END_EVENT_TABLE()

/*****************************************************************************
 * Public methods.
 *****************************************************************************/
wxWindow *VideoWindow( intf_thread_t *p_intf, wxWindow *p_parent )
{
    return new VideoWindow::VideoWindow( p_intf, p_parent );
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
    b_in_use = VLC_FALSE;

    p_intf->pf_request_window = ::GetWindow;
    p_intf->pf_release_window = ::ReleaseWindow;

    p_intf->p_sys->p_video_window = this;
    p_child_window = new wxWindow( this, -1, wxDefaultPosition, wxSize(0,0) );
    p_child_window->Show();
    Show();

    p_intf->p_sys->p_video_sizer = new wxBoxSizer( wxHORIZONTAL );
    p_intf->p_sys->p_video_sizer->Add( this, 1, wxEXPAND );

    ReleaseWindow( NULL );
}

VideoWindow::~VideoWindow()
{
    vlc_mutex_destroy( &lock );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
static void *GetWindow( intf_thread_t *p_intf, int *pi_x_hint, int *pi_y_hint,
                        unsigned int *pi_width_hint,
                        unsigned int *pi_height_hint )
{
    return p_intf->p_sys->p_video_window->GetWindow( pi_x_hint, pi_y_hint,
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

void *VideoWindow::GetWindow( int *pi_x_hint, int *pi_y_hint,
                              unsigned int *pi_width_hint,
                              unsigned int *pi_height_hint )
{
#if defined(__WXGTK__) || defined(WIN32)
    vlc_mutex_lock( &lock );

    if( b_in_use )
    {
        msg_Dbg( p_intf, "Video window already in use" );
        return NULL;
    }

    b_in_use = VLC_TRUE;

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
    return (void *)( (char *)gtk_widget_get_parent_window( p_widget )
               + 2 * sizeof(void *) );
#endif

#elif defined(WIN32)
    return GetHandle();

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

    b_in_use = VLC_FALSE;

#if defined(__WXGTK__) || defined(WIN32)
    wxSizeEvent event( wxSize(0, 0), UpdateHide_Event );
    AddPendingEvent( event );
#endif

    vlc_mutex_unlock( &lock );
}

void VideoWindow::UpdateSize( wxSizeEvent &event )
{
    if( !IsShown() )
    {
        p_intf->p_sys->p_video_sizer->Show( this, TRUE );
        p_intf->p_sys->p_video_sizer->Layout();
        SetFocus();
    }
    p_intf->p_sys->p_video_sizer->SetMinSize( event.GetSize() );

    wxCommandEvent intf_event( wxEVT_INTF, 0 );
    p_parent->AddPendingEvent( intf_event );
}

void VideoWindow::UpdateHide( wxSizeEvent &event )
{
    if( IsShown() )
    {
        p_intf->p_sys->p_video_sizer->Show( this, FALSE );
        p_intf->p_sys->p_video_sizer->Layout();
    }
    p_intf->p_sys->p_video_sizer->SetMinSize( event.GetSize() );

    wxCommandEvent intf_event( wxEVT_INTF, 0 );
    p_parent->AddPendingEvent( intf_event );
}
