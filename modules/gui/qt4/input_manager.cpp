/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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
    old_name="";
    p_input = NULL;
    i_rate = 0;
    ON_TIMEOUT( update() );
}

InputManager::~InputManager()
{
    delInput();
}

void InputManager::setInput( input_thread_t *_p_input )
{
    delInput();
    p_input = _p_input;
    emit positionUpdated( 0.0,0,0 );
    b_had_audio = b_had_video = b_has_audio = b_has_video = false;
    if( p_input )
    {
        vlc_object_yield( p_input );
        vlc_value_t val;
        var_Change( p_input, "video-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
        b_has_video = val.i_int > 0;
        var_Change( p_input, "audio-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
        b_has_audio = val.i_int > 0;
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
        vlc_object_release( p_input );
        p_input = NULL;
    }
}

void InputManager::update()
{
    /// \todo Emit the signals only if it changed
    if( !p_input ) return;

    if( p_input->b_dead || p_input->b_die )
    {
        emit positionUpdated( 0.0, 0, 0 );
        emit navigationChanged( 0 );
        i_old_playing_status = 0;
        emit statusChanged( END_S ); // see vlc_input.h, input_state_e enum
        delInput();
        return;
    }

    /* Update position */
    mtime_t i_length, i_time;
    float f_pos;
    i_length = var_GetTime( p_input, "length" ) / 1000000;
    i_time = var_GetTime( p_input, "time") / 1000000;
    f_pos = var_GetFloat( p_input, "position" );
    emit positionUpdated( f_pos, i_time, i_length );
 
    int i_new_rate = var_GetInteger( p_input, "rate");
    if( i_new_rate != i_rate )
    {
        i_rate = i_new_rate;
        /* Update rate */
        emit rateChanged( i_rate );
    }

    /* Update navigation status */
    vlc_value_t val; val.i_int = 0;
    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int > 0 )
    {
        val.i_int = 0;
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int > 0 )
        {
            emit navigationChanged( 1 ); // 1 = chapter, 2 = title, 0 = NO
        }
        else
        {
            emit navigationChanged( 2 );
        }
    }
    else
    {
        emit navigationChanged( 0 );
    }

    /* Update text */
    QString text;
    char *psz_name = input_item_GetTitle( input_GetItem( p_input ) );
    char *psz_nowplaying =
        input_item_GetNowPlaying( input_GetItem( p_input ) );
    char *psz_artist = input_item_GetArtist( input_GetItem( p_input ) );
    if( EMPTY_STR( psz_name ) )
    {
        free( psz_name );
        psz_name = input_item_GetName( input_GetItem( p_input ) );
    }
    if( !EMPTY_STR( psz_nowplaying ) )
    {
        text.sprintf( "%s - %s", psz_nowplaying, psz_name );
    }
    else if( !EMPTY_STR( psz_artist ) )
    {
        text.sprintf( "%s - %s", psz_artist, psz_name );
    }
    else
    {
        text.sprintf( "%s", psz_name );
    }
    free( psz_name );
    free( psz_nowplaying );
    free( psz_artist );
    if( old_name != text )
    {
        emit nameChanged( text );
        old_name=text;
    }
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
    if( hasInput() )
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

void InputManager::sectionPrev()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        vlc_value_t val; val.b_bool = VLC_TRUE;
        var_Set( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                            "prev-chapter":"prev-title", val );
    }
}

void InputManager::sectionNext()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        vlc_value_t val; val.b_bool = VLC_TRUE;
        var_Set( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                            "next-chapter":"next-title", val );
    }
}

void InputManager::sectionMenu()
{
    if( hasInput() )
        var_SetInteger( p_input, "title 0", 2 );
}

void InputManager::slower()
{
    if( hasInput() )
        var_SetVoid( p_input, "rate-slower" );
}

void InputManager::faster()
{
    if( hasInput() )
        var_SetVoid( p_input, "rate-faster" );
}

void InputManager::normalRate()
{
    if( hasInput() )
        var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT );
}

void InputManager::setRate( int new_rate )
{
    if( hasInput() )
        var_SetInteger( p_input, "rate", new_rate );
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
    if( VLC_OBJECT_INTF == p_intf->i_object_type )
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
    else {
        /* we are working as a dialogs provider */
        playlist_t *p_playlist = (playlist_t *) vlc_object_find( p_intf,
                                       VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
            p_input = p_playlist->p_input;
            emit inputChanged( p_input );
        }
    }
}

void MainInputManager::stop()
{
   playlist_Stop( THEPL );
}

void MainInputManager::next()
{
   playlist_Next( THEPL );
}

void MainInputManager::prev()
{
   playlist_Prev( THEPL );
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
    return VLC_SUCCESS;
}

static int ChangeVideo( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param )
{
    InputManager *im = (InputManager*)param;
    im->b_has_video = true;
    return VLC_SUCCESS;
}
