/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakka  <ileoo@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"

static int ChangeVideo( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param );
static int ChangeAudio( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param );
static int ItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int InterfaceChanged( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void * );
static int ItemStateChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int ItemRateChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int ItemTitleChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int VolumeChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );

/**********************************************************************
 * InputManager implementation
 **********************************************************************/

InputManager::InputManager( QObject *parent, intf_thread_t *_p_intf) :
                           QObject( parent ), p_intf( _p_intf )
{
    i_old_playing_status = END_S;
    old_name = "";
    p_input = NULL;
    i_rate = 0;
    var_AddCallback( THEPL, "playlist-current", ItemChanged, this );
    var_AddCallback( THEPL, "intf-change", ItemChanged, this );
}

InputManager::~InputManager()
{
    delInput();
    var_DelCallback( THEPL, "playlist-current", ItemChanged, this );
    var_DelCallback( THEPL, "intf-change", ItemChanged, this );
}

void InputManager::setInput( input_thread_t *_p_input )
{
    delInput();
    p_input = _p_input;
    b_had_audio = b_had_video = b_has_audio = b_has_video = false;
    if( p_input )
    {
        vlc_object_yield( p_input );
        vlc_value_t val;
        var_Change( p_input, "video-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
        b_has_video = val.i_int > 0;
        var_Change( p_input, "audio-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
        b_has_audio = val.i_int > 0;
        emit statusChanged( PLAYING_S );
        addCallbacks();
    }
}

void InputManager::delInput()
{
    if( p_input )
    {
        delCallbacks();
        vlc_object_release( p_input );
        p_input = NULL;
    }
    i_old_playing_status = END_S;
    old_name=qfu("");
    artUrl = qfu("");
    emit positionUpdated( 0.0, 0 ,0 );
    emit statusChanged( END_S );
    emit nameChanged( "" );
    emit artChanged( "" );
}

void InputManager::delCallbacks( void )
{
    var_DelCallback( p_input, "audio-es", ChangeAudio, this );
    var_DelCallback( p_input, "video-es", ChangeVideo, this );
    var_DelCallback( THEPL, "state", ItemStateChanged, this );
    var_DelCallback( p_input, "rate", ItemRateChanged, this );
    var_DelCallback( p_input, "title", ItemTitleChanged, this );
    var_DelCallback( p_input, "intf-change", InterfaceChanged, this );
    var_DelCallback( THEPL, "item-change", ItemChanged, this );
}

void InputManager::addCallbacks( void )
{
    var_AddCallback( p_input, "audio-es", ChangeAudio, this );
    var_AddCallback( p_input, "video-es", ChangeVideo, this );
    /* src/playlist/control.c */
    var_AddCallback( THEPL, "state", ItemStateChanged, this );
    /* src/input/input.c:1765 */
    var_AddCallback( p_input, "rate", ItemRateChanged, this );
    /* src/input/input.c:2003 */
    var_AddCallback( p_input, "title", ItemTitleChanged, this );
    /* src/input/input.c:734 for timers update*/
    var_AddCallback( p_input, "intf-change", InterfaceChanged, this );
    /* src/input/input.c:2076*/
    var_AddCallback( THEPL, "item-change", ItemChanged, this );
}

static int InterfaceChanged( vlc_object_t *p_this, const char *psz_var,
                            vlc_value_t oldval, vlc_value_t newval, void *param )
{
    static int counter = 0;
    InputManager *im = (InputManager*)param;

    counter = counter++ % 4;
    if(!counter)
        return VLC_SUCCESS;
    IMEvent *event = new IMEvent( PositionUpdate_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemStateChanged( vlc_object_t *p_this, const char *psz_var,
                            vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemStateChanged_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemRateChanged( vlc_object_t *p_this, const char *psz_var,
                            vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemRateChanged_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemTitleChanged( vlc_object_t *p_this, const char *psz_var,
                            vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemTitleChanged_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int InputChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MainInputManager *im = (MainInputManager*)param;

    IMEvent *event = new IMEvent( ItemChanged_Type, newval.i_int );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int VolumeChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MainInputManager *im = (MainInputManager*)param;

    IMEvent *event = new IMEvent( VolumeChanged_Type, newval.i_int );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemChanged_Type, newval.i_int );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

void InputManager::customEvent( QEvent *event )
{
    int type = event->type();
    if ( type != PositionUpdate_Type &&  type != ItemChanged_Type &&
         type != ItemRateChanged_Type && type != ItemTitleChanged_Type &&
         type != ItemStateChanged_Type )
        return;

    if(!p_input || p_input->b_dead || p_input->b_die )
    {
         delInput();
         return;
    }

    IMEvent *ime = static_cast<IMEvent *>(event);

    if ( type == PositionUpdate_Type )
    {
        UpdatePosition();
    }
    else if ( type == ItemChanged_Type )
    {
        UpdateMeta();
        UpdateTitle();
    }
    else if ( type == ItemRateChanged_Type )
    {
       UpdateRate();
    }
    else if ( type == ItemTitleChanged_Type )
    {
       UpdateTitle();
    }
    else if (type == ItemStateChanged_Type )
    {
       UpdateStatus();
    }
}

void InputManager::UpdatePosition( void )
{
     /* Update position */
     int i_length, i_time; /* Int is enough, since we store seconds */
     float f_pos;
     i_length = var_GetTime(  p_input , "length" ) / 1000000;
     i_time = var_GetTime(  p_input , "time") / 1000000;
     f_pos = var_GetFloat(  p_input , "position" );
     emit positionUpdated( f_pos, i_time, i_length );
}

void InputManager::UpdateTitle( void )
{
     /* Update navigation status */
     vlc_value_t val; val.i_int = 0;
     var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
     msg_Dbg( p_intf, "updateTitle called" );
     if( val.i_int > 0 )
     {
         val.i_int = 0;
         var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
         emit navigationChanged( (val.i_int > 0) ? 1 : 2 );
     }
     else
     {
         emit navigationChanged( 0 );
     }
}

void InputManager::UpdateStatus( void )
{
     /* Update playing status */
     vlc_value_t val; val.i_int = 0;
     var_Get( p_input, "state", &val );
     if( i_old_playing_status != val.i_int )
     {
         i_old_playing_status = val.i_int;
         emit statusChanged( val.i_int );
     }
}

void InputManager::UpdateRate( void )
{
     /* Update Rate */
     int i_new_rate = var_GetInteger( p_input, "rate");
     if( i_new_rate != i_rate )
     {
         i_rate = i_new_rate;
         /* Update rate */
         emit rateChanged( i_rate );
     }
}

void InputManager::UpdateMeta( void )
{
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

     QString url;
     char *psz_art = input_item_GetArtURL( input_GetItem( p_input ) );
     url.sprintf("%s", psz_art );
     free( psz_art );
     if( artUrl != url )
     {
         artUrl = url.replace( "file://",QString("" ) );
         emit artChanged( artUrl );
     }
#ifdef ZVBI_COMPILED
     /* Update teletext status*/
     emit teletextEnabled( true );/* FIXME */
#endif

}

void InputManager::sliderUpdate( float new_pos )
{
    if( hasInput() ) var_SetFloat( p_input, "position", new_pos );
}

void InputManager::togglePlayPause()
{
    vlc_value_t state;
    var_Get( p_input, "state", &state );
    state.i_int = ( ( state.i_int != PLAYING_S ) ? PLAYING_S : PAUSE_S );
    msg_Dbg( p_input, "state : %d", state.i_int );
    /*{
        /* A stream is being played, pause it */
       /* state.i_int = PAUSE_S;
    }
    else
    {
        /* Stream is paused, resume it */
        /*state.i_int = PLAYING_S;
    }*/
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

#ifdef ZVBI_COMPILED
void InputManager::telexGotoPage( int page )
{
    if( hasInput() )
    {
        vlc_object_t *p_vbi;
        p_vbi = (vlc_object_t *) vlc_object_find_name( p_input,
                    "zvbi", FIND_ANYWHERE );
        if( p_vbi )
        {
            var_SetInteger( p_vbi, "vbi-page", page );
            vlc_object_release( p_vbi );
        }
    }
}

void InputManager::telexToggle( bool b_enabled )
{
    int i_page = 0;

    if( b_enabled )
        i_page = 100;
    telexGotoPage( i_page );
}

void InputManager::telexSetTransparency( bool b_transp )
{
    if( hasInput() )
    {
        vlc_object_t *p_vbi;
        p_vbi = (vlc_object_t *) vlc_object_find_name( p_input,
                    "zvbi", FIND_ANYWHERE );
        if( p_vbi )
        {
            var_SetBool( p_input->p_libvlc, "vbi-opaque", b_transp );
            vlc_object_release( p_vbi );
        }
    }
}
#endif

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
    var_AddCallback( THEPL, "playlist-current", InputChanged, this );
    var_AddCallback( THEPL, "activity", InputChanged, this );
    var_AddCallback( p_intf->p_libvlc, "volume-change", VolumeChanged, this );
    /* Warn our embedded IM about input changes */
    CONNECT( this, inputChanged( input_thread_t * ),
             im, setInput( input_thread_t * ) );
}

MainInputManager::~MainInputManager()
{
    if( p_input )
    {
       vlc_object_release( p_input );
       emit inputChanged( NULL );
    }
    var_DelCallback( p_intf->p_libvlc, "volume-change", VolumeChanged, this );
    var_DelCallback( THEPL, "playlist-current", InputChanged, this );
    var_DelCallback( THEPL, "activity", InputChanged, this );
}

void MainInputManager::customEvent( QEvent *event )
{
    int type = event->type();
    if ( type != ItemChanged_Type && type != VolumeChanged_Type )
        return;

    if( type == VolumeChanged_Type )
    {
        emit volumeChanged();
        return;
    }

    if( VLC_OBJECT_INTF == p_intf->i_object_type )
    {
        vlc_mutex_lock( &p_intf->change_lock );
        if( p_input && ( p_input->b_dead || p_input->b_die ) )
        {
            var_DelCallback( p_input, "state", InputChanged, this );
            vlc_object_release( p_input );
            p_input = NULL;
            emit inputChanged( NULL );
        }

        if( !p_input )
        {
            QPL_LOCK;
            p_input = THEPL->p_input;
            if( p_input && !( p_input->b_die || p_input->b_dead) )
            {
                vlc_object_yield( p_input );
                var_AddCallback( p_input, "state", InputChanged, this );
                emit inputChanged( p_input );
            }
            else
                p_input = NULL;
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
            vlc_object_yield( p_input );
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

