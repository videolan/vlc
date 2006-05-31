/*****************************************************************************
 * main_inteface.cpp : Main interface
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#include "main_interface.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include <QCloseEvent>
#include <assert.h>

MainInterface::MainInterface( intf_thread_t *_p_intf ) :
                            QWidget( NULL ), p_intf( _p_intf)
{
    fprintf( stderr, "QT Main interface\n" );

    /* Init UI */

    /* Init input manager */
    p_input = NULL;
    main_input_manager = new InputManager( this, p_intf );
}

void MainInterface::init()
{
    /* Get timer updates */
    QObject::connect( DialogsProvider::getInstance(NULL)->fixed_timer,
                      SIGNAL( timeout() ), this, SLOT(updateOnTimer() ) );
    /* Tell input manager about the input changes */
    QObject::connect( this, SIGNAL( inputChanged( input_thread_t * ) ),
                   main_input_manager, SLOT( setInput( input_thread_t * ) ) );

    /* Connect the slider and the input manager */
    // both ways 

    /* Connect the display and the input manager */
}

MainInterface::~MainInterface()
{
}

void MainInterface::updateOnTimer()
{
    if( p_intf->b_die )
    {
        QApplication::quit();
    }
    vlc_mutex_lock( &p_intf->change_lock );
    if( p_input && p_input->b_dead )
    {
        vlc_object_release( p_input );
        p_input = NULL;
        emit inputChanged( NULL );
    }

    if( !p_input )
    {
        playlist_t *p_playlist = (playlist_t *) vlc_object_find( p_intf,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        assert( p_playlist );
        PL_LOCK;

        p_input = p_playlist->p_input;
        if( p_input )
        {
            vlc_object_yield( p_input );
            fprintf( stderr, "Sending input\n");
            emit inputChanged( p_input );
        }

        PL_UNLOCK;
        vlc_object_release( p_playlist );
    }
    vlc_mutex_unlock( &p_intf->change_lock );
}

void MainInterface::closeEvent( QCloseEvent *e )
{
    hide();
    p_intf->b_die = VLC_TRUE;
}
