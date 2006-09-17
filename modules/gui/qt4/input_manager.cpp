/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
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

#include <assert.h>

#include "qt4.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"

static int ChangeVideo( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param );
static int ChangeAudio( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param );

/**********************************************************************
 * InputManager implementation
 **********************************************************************/

InputManager::InputManager( QObject *parent, intf_thread_t *_p_intf) :
                           QObject( parent ), p_intf( _p_intf )
{
    i_old_playing_status = END_S;
    p_input = NULL;
    ON_TIMEOUT( update() );
}

InputManager::~InputManager()
{
}

void InputManager::setInput( input_thread_t *_p_input )
{
    p_input = _p_input;
    emit positionUpdated( 0.0,0,0 );
    b_had_audio = b_had_video = b_has_audio = b_has_video = false;
    if( p_input )
    {
        var_AddCallback( p_input, "audio-es", ChangeAudio, this );
        var_AddCallback( p_input, "video-es", ChangeVideo, this );
    }

}
void InputManager::delInput()
{
    if( p_input )
    {
        var_DelCallback( p_input, "audio-es", ChangeAudio, this );
        var_DelCallback( p_input, "video-es", ChangeVideo, this );
    }
}

void InputManager::update()
{
    /// \todo Emit the signals only if it changed
    if( !p_input  ) return;

    if( p_input->b_dead || p_input->b_die )
    {
        emit positionUpdated( 0.0, 0, 0 );
        emit navigationChanged( 0 );
        emit statusChanged( 0 ); // 0 = STOPPED, 1 = PLAY, 2 = PAUSE
    }

    if( !b_had_audio && b_has_audio )
        emit audioStarted();
    if( !b_had_video && b_has_video )
        emit videoStarted();

    /* Update position */
    mtime_t i_length, i_time;
    float f_pos;
    i_length = var_GetTime( p_input, "length" ) / 1000000;
    i_time = var_GetTime( p_input, "time") / 1000000;
    f_pos = var_GetFloat( p_input, "position" );
    emit positionUpdated( f_pos, i_time, i_length );

    /* Update disc status */
    vlc_value_t val;
    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int > 0 )
    {
        vlc_value_t val;
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int > 0 )
            emit navigationChanged( 1 ); // 1 = chapter, 2 = title, 0 = NO
        else
            emit navigationChanged( 2 );
    }
    else
    {
        emit navigationChanged( 0 );
    }

    /* Update text */
    QString text;
    if( p_input->input.p_item->p_meta &&
        p_input->input.p_item->p_meta->psz_nowplaying &&
        *p_input->input.p_item->p_meta->psz_nowplaying )
    {
        text.sprintf( "%s - %s",
                  p_input->input.p_item->p_meta->psz_nowplaying,
                  p_input->input.p_item->psz_name );
    }
    else
    {
        text.sprintf( "%s", p_input->input.p_item->psz_name );
    }
    emit nameChanged( text );

    /* Update playing status */
    var_Get( p_input, "state", &val );
    val.i_int = val.i_int == PAUSE_S ? PAUSE_S : PLAYING_S;
    if( i_old_playing_status != val.i_int )
    {
        i_old_playing_status = val.i_int;
        emit statusChanged(  val.i_int == PAUSE_S ? PAUSE_S : PLAYING_S );
    }
}

void InputManager::sliderUpdate( float new_pos )
{
    if( p_input && !p_input->b_die && !p_input->b_dead )
        var_SetFloat( p_input, "position", new_pos );
}

void InputManager::togglePlayPause()
{
    vlc_value_t state;
    var_Get( p_input, "state", &state );
    if( state.i_int != PAUSE_S )
    {
        /* A stream is being played, pause it */
        state.i_int = PAUSE_S;
    }
    else
    {
        /* Stream is paused, resume it */
        state.i_int = PLAYING_S;
    }
    var_Set( p_input, "state", state );
    emit statusChanged( state.i_int );
}

/**********************************************************************
 * MainInputManager implementation. Wrap an input manager and
 * take care of updating the main playlist input
 **********************************************************************/
MainInputManager * MainInputManager::instance = NULL;

MainInputManager::MainInputManager( intf_thread_t *_p_intf ) : QObject(NULL),
                                                p_intf( _p_intf )
{
    p_input = NULL;
    im = new InputManager( this, p_intf );
    ON_TIMEOUT( updateInput() );
    /* Warn our embedded IM about input changes */
    CONNECT( this, inputChanged( input_thread_t * ),
             im,   setInput( input_thread_t * ) );
}

MainInputManager::~MainInputManager()
{
    if( p_input ) vlc_object_release( p_input );
}

void MainInputManager::updateInput()
{
    vlc_mutex_lock( &p_intf->change_lock );
    if( p_input && p_input->b_dead )
    {
        vlc_object_release( p_input );
        getIM()->delInput();
        p_input = NULL;
        emit inputChanged( NULL );
    }

    if( !p_input )
    {
        QPL_LOCK;
        p_input = THEPL->p_input;
        if( p_input )
        {
            vlc_object_yield( p_input );
            emit inputChanged( p_input );
        }
        QPL_UNLOCK;
    }
    vlc_mutex_unlock( &p_intf->change_lock );
}

void MainInputManager::togglePlayPause()
{
    if( p_input == NULL )
    {
        playlist_Play( THEPL );
        return;
    }
    getIM()->togglePlayPause();
}


static int ChangeAudio( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param )
{
    InputManager *im = (InputManager*)param;
    im->b_has_audio = true;
}

static int ChangeVideo( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param )
{
    InputManager *im = (InputManager*)param;
    im->b_has_video = true;
}
