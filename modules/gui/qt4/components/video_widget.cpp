/*****************************************************************************
 * video_widget.cpp : Embedded video output
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "dialogs_provider.hpp"
#include <vlc/vout.h>
#include "qt4.hpp"
#include "components/video_widget.hpp"
#include "main_interface.hpp"

static void *DoRequest( intf_thread_t *, vout_thread_t *, int*,int*,
                        unsigned int *, unsigned int * );
static void DoRelease( intf_thread_t *, void * );
static int DoControl( intf_thread_t *, void *, int, va_list );

bool need_update;
       
VideoWidget::VideoWidget( intf_thread_t *_p_i ) : QFrame( NULL ),
                                                              p_intf( _p_i )
{
    vlc_mutex_init( p_intf, &lock );

    p_intf->pf_request_window  = ::DoRequest;
    p_intf->pf_release_window  = ::DoRelease;
    p_intf->pf_control_window  = ::DoControl;
    p_intf->p_sys->p_video = this;
    p_vout = NULL;

    setFrameStyle(QFrame::Panel | QFrame::Raised);

    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );

    connect( DialogsProvider::getInstance(NULL)->fixed_timer,
             SIGNAL( timeout() ), this, SLOT( update() ) );

    need_update = false;
}

void VideoWidget::update()
{
    if( need_update )
    {
        p_intf->p_sys->p_mi->resize( p_intf->p_sys->p_mi->sizeHint() );
        need_update = false;
    }
}

VideoWidget::~VideoWidget()
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

QSize VideoWidget::sizeHint() const
{
    return p_intf->p_sys->p_mi->videoSize;
}

static void *DoRequest( intf_thread_t *p_intf, vout_thread_t *p_vout,
                        int *pi1, int *pi2, unsigned int*pi3,unsigned int*pi4)
{
    return p_intf->p_sys->p_video->Request( p_vout, pi1, pi2, pi3, pi4 );
}

void *VideoWidget::Request( vout_thread_t *p_nvout, int *pi_x, int *pi_y,
                           unsigned int *pi_width, unsigned int *pi_height )
{
    if( p_vout )
    {
        msg_Dbg( p_intf, "embedded video already in use" );
        return NULL;
    }
    p_vout = p_nvout;

    fprintf( stderr, "[Before update] MI constraints %ix%i -> %ix%i\n",
                    p_intf->p_sys->p_mi->minimumSize().width(),
                    p_intf->p_sys->p_mi->minimumSize().height(),
                    p_intf->p_sys->p_mi->maximumSize().width(),
                    p_intf->p_sys->p_mi->maximumSize().height() );

    setMinimumSize( 1,1 );
    p_intf->p_sys->p_mi->videoSize = QSize( *pi_width, *pi_height );
    updateGeometry();
    need_update = true;
    fprintf( stderr, "[After update] MI constraints %ix%i -> %ix%i - Fr %ix%i -> %ix%i (hint %ix%i)\n",
                    p_intf->p_sys->p_mi->minimumSize().width(),
                    p_intf->p_sys->p_mi->minimumSize().height(),
                    p_intf->p_sys->p_mi->maximumSize().width(),
                    p_intf->p_sys->p_mi->maximumSize().height(),
                    minimumSize().width(),
                    minimumSize().height(),
                    maximumSize().width(),
                    maximumSize().height(),
                    sizeHint().width(),sizeHint().height() 
           );
    
    return  (void*)winId();
}

static void DoRelease( intf_thread_t *p_intf, void *p_win )
{
    return p_intf->p_sys->p_video->Release( p_win );
}

void VideoWidget::Release( void *p_win )
{
    if( !config_GetInt( p_intf, "qt-always-video" ) );
    {
        p_intf->p_sys->p_mi->videoSize = QSize ( 1,1 );
    }
    fprintf( stderr, "[Before R update] MI constraints %ix%i -> %ix%i\n",
                    p_intf->p_sys->p_mi->minimumSize().width(),
                    p_intf->p_sys->p_mi->minimumSize().height(),
                    p_intf->p_sys->p_mi->maximumSize().width(),
                    p_intf->p_sys->p_mi->maximumSize().height() );

    updateGeometry();

//    p_intf->p_sys->p_mi->setMinimumSize( 500,
//                                       p_intf->p_sys->p_mi->addSize.height() );
    if( !config_GetIntf( p_intf, "qt-always-video" ) )
        need_update = true;

    fprintf( stderr, "[After R update] MI constraints %ix%i -> %ix%i\n",
                    p_intf->p_sys->p_mi->minimumSize().width(),
                    p_intf->p_sys->p_mi->minimumSize().height(),
                    p_intf->p_sys->p_mi->maximumSize().width(),
                    p_intf->p_sys->p_mi->maximumSize().height() );
    
    p_vout = NULL;
}


static int DoControl( intf_thread_t *p_intf, void *p_win, int i_q, va_list a )
{
    return p_intf->p_sys->p_video->Control( p_win, i_q, a );
} 

int VideoWidget::Control( void *p_window, int i_query, va_list args )
{
    int i_ret = VLC_EGENERIC;
    vlc_mutex_lock( &lock );
    switch( i_query )
    {
        case VOUT_GET_SIZE:
        {
            unsigned int *pi_width  = va_arg( args, unsigned int * );
            unsigned int *pi_height = va_arg( args, unsigned int * );
            *pi_width = frame->width();
            *pi_height = frame->height();
            i_ret = VLC_SUCCESS;
            break;
        }
        case VOUT_SET_SIZE:
        {
            unsigned int i_width  = va_arg( args, unsigned int );
            unsigned int i_height = va_arg( args, unsigned int );

            if( !i_width && p_vout ) i_width = p_vout->i_window_width;
            if( !i_height && p_vout ) i_height = p_vout->i_window_height;
           
            frame->resize( i_width, i_height );
            i_ret = VLC_SUCCESS;
            break; 
        }
        case VOUT_SET_STAY_ON_TOP:
        {
            /// \todo
            break;
        }   
        default:
            msg_Warn( p_intf, "unsupported control query" );
            break;
    }
    vlc_mutex_unlock( &lock );
    return i_ret;
}
