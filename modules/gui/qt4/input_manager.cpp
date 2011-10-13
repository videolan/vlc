/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakka  <ileoo@videolan.org>
 *          Jean-Baptiste <jb@videolan.org>
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

#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "input_manager.hpp"
#include <vlc_keys.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_aout.h>
#include <vlc_aout_intf.h>

#include <QApplication>

#include <assert.h>

static int ItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int LeafToParent( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int PLItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int PLItemAppended( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int PLItemRemoved( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int VolumeChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int SoundMuteChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );

static int RandomChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int LoopChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int RepeatChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );


static int InputEvent( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );
static int VbiEvent( vlc_object_t *, const char *,
                     vlc_value_t, vlc_value_t, void * );


/**********************************************************************
 * InputManager implementation
 **********************************************************************
 * The Input Manager can be the main one around the playlist
 * But can also be used for VLM dialog or similar
 **********************************************************************/

InputManager::InputManager( QObject *parent, intf_thread_t *_p_intf) :
                           QObject( parent ), p_intf( _p_intf )
{
    i_old_playing_status = END_S;
    oldName      = "";
    artUrl       = "";
    p_input      = NULL;
    p_input_vbi  = NULL;
    f_rate       = 0.;
    p_item       = NULL;
    b_video      = false;
    timeA        = 0;
    timeB        = 0;
    f_cache      = -1.; /* impossible initial value, different from all */
}

InputManager::~InputManager()
{
    delInput();
}

/* Define the Input used.
   Add the callbacks on input
   p_input is held once here */
void InputManager::setInput( input_thread_t *_p_input )
{
    delInput();
    p_input = _p_input;
    if( p_input && !( p_input->b_dead || !vlc_object_alive (p_input) ) )
    {
        msg_Dbg( p_intf, "IM: Setting an input" );
        vlc_object_hold( p_input );
        addCallbacks();
        UpdateStatus();
        UpdateName();
        UpdateArt();
        UpdateTeletext();
        UpdateNavigation();
        UpdateVout();

        p_item = input_GetItem( p_input );
        emit rateChanged( var_GetFloat( p_input, "rate" ) );
    }
    else
    {
        p_input = NULL;
        p_item = NULL;
        assert( !p_input_vbi );
        emit rateChanged( var_InheritFloat( p_intf, "rate" ) );
    }
}

/* delete Input if it ever existed.
   Delete the callbacls on input
   p_input is released once here */
void InputManager::delInput()
{
    if( !p_input ) return;
    msg_Dbg( p_intf, "IM: Deleting the input" );

    delCallbacks();
    i_old_playing_status = END_S;
    p_item               = NULL;
    oldName              = "";
    artUrl               = "";
    b_video              = false;
    timeA                = 0;
    timeB                = 0;
    f_rate               = 0. ;

    if( p_input_vbi )
    {
        vlc_object_release( p_input_vbi );
        p_input_vbi = NULL;
    }

    vlc_object_release( p_input );
    p_input = NULL;

    emit positionUpdated( -1.0, 0 ,0 );
    emit rateChanged( var_InheritFloat( p_intf, "rate" ) );
    emit nameChanged( "" );
    emit chapterChanged( 0 );
    emit titleChanged( 0 );
    emit playingStatusChanged( END_S );

    emit teletextPossible( false );
    emit AtoBchanged( false, false );
    emit voutChanged( false );
    emit voutListChanged( NULL, 0 );

    /* Reset all InfoPanels but stats */
    emit artChanged( NULL );
    emit infoChanged( NULL );
    emit currentMetaChanged( (input_item_t *)NULL );

    emit encryptionChanged( false );
    emit recordingStateChanged( false );

    emit cachingChanged( 1 );
}

/* Convert the event from the callbacks in actions */
void InputManager::customEvent( QEvent *event )
{
    int i_type = event->type();
    IMEvent *ple = static_cast<IMEvent *>(event);

    if( i_type == ItemChanged_Type )
        UpdateMeta( ple->p_item );

    if( !hasInput() )
        return;

    /* Actions */
    switch( i_type )
    {
    case PositionUpdate_Type:
        UpdatePosition();
        break;
    case StatisticsUpdate_Type:
        UpdateStats();
        break;
    case ItemChanged_Type:
        /* Ignore ItemChanged_Type event that does not apply to our input */
        if( p_item == ple->p_item )
        {
            UpdateStatus();
            // UpdateName();
            UpdateArt();
            /* Update duration of file */
        }
        break;
    case ItemStateChanged_Type:
        // TODO: Fusion with above state
        UpdateStatus();
        // UpdateName();
        // UpdateNavigation(); This shouldn't be useful now
        // UpdateTeletext(); Same
        break;
    case NameChanged_Type:
        UpdateName();
        break;
    case MetaChanged_Type:
        UpdateMeta();
        UpdateName(); /* Needed for NowPlaying */
        UpdateArt(); /* Art is part of meta in the core */
        break;
    case InfoChanged_Type:
        UpdateInfo();
        break;
    case ItemTitleChanged_Type:
        UpdateNavigation();
        UpdateName(); /* Display the name of the Chapter, if exists */
        break;
    case ItemRateChanged_Type:
        UpdateRate();
        break;
    case ItemEsChanged_Type:
        UpdateTeletext();
        // We don't do anything ES related. Why ?
        break;
    case ItemTeletextChanged_Type:
        UpdateTeletext();
        break;
    case InterfaceVoutUpdate_Type:
        UpdateVout();
        break;
    case SynchroChanged_Type:
        emit synchroChanged();
        break;
    case CachingEvent_Type:
        UpdateCaching();
        break;
    case BookmarksChanged_Type:
        emit bookmarksChanged();
        break;
    case InterfaceAoutUpdate_Type:
        UpdateAout();
        break;
    case RecordingEvent_Type:
        UpdateRecord();
        break;
    case ProgramChanged_Type:
        UpdateProgramEvent();
        break;
    case EPGEvent_Type:
        UpdateEPG();
        break;
    default:
        msg_Warn( p_intf, "This shouldn't happen: %i", i_type );
        assert(0);
    }
}

/* Add the callbacks on Input. Self explanatory */
inline void InputManager::addCallbacks()
{
    var_AddCallback( p_input, "intf-event", InputEvent, this );
}

/* Delete the callbacks on Input. Self explanatory */
inline void InputManager::delCallbacks()
{
    var_DelCallback( p_input, "intf-event", InputEvent, this );
}

/* Static callbacks for IM */
static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );

    InputManager *im = (InputManager*)param;
    input_item_t *p_item = static_cast<input_item_t *>(newval.p_address);

    IMEvent *event = new IMEvent( ItemChanged_Type, p_item );
    QApplication::postEvent( im, event );
    return VLC_SUCCESS;
}

static int InputEvent( vlc_object_t *p_this, const char *,
                       vlc_value_t, vlc_value_t newval, void *param )
{
    VLC_UNUSED( p_this );

    InputManager *im = (InputManager*)param;
    IMEvent *event;

    switch( newval.i_int )
    {
    case INPUT_EVENT_STATE:
        event = new IMEvent( ItemStateChanged_Type );
        break;
    case INPUT_EVENT_RATE:
        event = new IMEvent( ItemRateChanged_Type );
        break;
    case INPUT_EVENT_POSITION:
    //case INPUT_EVENT_LENGTH:
        event = new IMEvent( PositionUpdate_Type );
        break;

    case INPUT_EVENT_TITLE:
    case INPUT_EVENT_CHAPTER:
        event = new IMEvent( ItemTitleChanged_Type );
        break;

    case INPUT_EVENT_ES:
        event = new IMEvent( ItemEsChanged_Type );
        break;
    case INPUT_EVENT_TELETEXT:
        event = new IMEvent( ItemTeletextChanged_Type );
        break;

    case INPUT_EVENT_STATISTICS:
        event = new IMEvent( StatisticsUpdate_Type );
        break;

    case INPUT_EVENT_VOUT:
        event = new IMEvent( InterfaceVoutUpdate_Type );
        break;
    case INPUT_EVENT_AOUT:
        event = new IMEvent( InterfaceAoutUpdate_Type );
        break;

    case INPUT_EVENT_ITEM_META: /* Codec MetaData + Art */
        event = new IMEvent( MetaChanged_Type );
        break;
    case INPUT_EVENT_ITEM_INFO: /* Codec Info */
        event = new IMEvent( InfoChanged_Type );
        break;
    case INPUT_EVENT_ITEM_NAME:
        event = new IMEvent( NameChanged_Type );
        break;

    case INPUT_EVENT_AUDIO_DELAY:
    case INPUT_EVENT_SUBTITLE_DELAY:
        event = new IMEvent( SynchroChanged_Type );
        break;

    case INPUT_EVENT_CACHE:
        event = new IMEvent( CachingEvent_Type );
        break;

    case INPUT_EVENT_BOOKMARK:
        event = new IMEvent( BookmarksChanged_Type );
        break;

    case INPUT_EVENT_RECORD:
        event = new IMEvent( RecordingEvent_Type );
        break;

    case INPUT_EVENT_PROGRAM:
        /* This is for PID changes */
        event = new IMEvent( ProgramChanged_Type );
        break;

    case INPUT_EVENT_ITEM_EPG:
        /* EPG data changed */
        event = new IMEvent( EPGEvent_Type );
        break;

    case INPUT_EVENT_SIGNAL:
        /* This is for capture-card signals */
        /* event = new IMEvent( SignalChanged_Type );
        break; */
    default:
        event = NULL;
        break;
    }

    if( event )
        QApplication::postEvent( im, event );
    return VLC_SUCCESS;
}

static int VbiEvent( vlc_object_t *, const char *,
                     vlc_value_t, vlc_value_t, void *param )
{
    InputManager *im = (InputManager*)param;
    IMEvent *event = new IMEvent( ItemTeletextChanged_Type );

    QApplication::postEvent( im, event );
    return VLC_SUCCESS;
}

void InputManager::UpdatePosition()
{
    /* Update position */
    int i_length;
    int64_t i_time;
    float f_pos;
    i_length = var_GetTime(  p_input , "length" ) / CLOCK_FREQ;
    i_time = var_GetTime(  p_input , "time");
    f_pos = var_GetFloat(  p_input , "position" );
    emit positionUpdated( f_pos, i_time, i_length );
}

void InputManager::UpdateNavigation()
{
    /* Update navigation status */
    vlc_value_t val; val.i_int = 0;
    vlc_value_t val2; val2.i_int = 0;

    if( hasInput() )
        var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );

    if( val.i_int > 0 )
    {
        emit titleChanged( true );
        msg_Dbg( p_intf, "Title %"PRId64, val.i_int );
        /* p_input != NULL since val.i_int != 0 */
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val2, NULL );
        emit chapterChanged( (val2.i_int > 1) || ( val2.i_int > 0 && val.i_int > 1 ) );
        msg_Dbg( p_intf, "Chapter: %"PRId64, val2.i_int );
    }
    else
        emit titleChanged( false );
}

void InputManager::UpdateStatus()
{
    /* Update playing status */
    int state = var_GetInteger( p_input, "state" );
    if( i_old_playing_status != state )
    {
        i_old_playing_status = state;
        emit playingStatusChanged( state );
    }
}

void InputManager::UpdateRate()
{
    /* Update Rate */
    float f_new_rate = var_GetFloat( p_input, "rate" );
    if( f_new_rate != f_rate )
    {
        f_rate = f_new_rate;
        /* Update rate */
        emit rateChanged( f_rate );
    }
}

void InputManager::UpdateName()
{
    /* Update text, name and nowplaying */
    QString name;

    /* Try to get the nowplaying */
    char *format = var_InheritString( p_intf, "input-title-format" );
    char *formated = str_format_meta( p_input, format );
    free( format );
    name = qfu(formated);
    free( formated );

    /* If we have Nothing */
    if( name.isEmpty() )
    {
        char *uri = input_item_GetURI( input_GetItem( p_input ) );
        char *file = uri ? strrchr( uri, '/' ) : NULL;
        if( file != NULL )
        {
            decode_URI( ++file );
            name = qfu(file);
        }
        else
            name = qfu(uri);
        free( uri );
    }

    name = name.trimmed();

    if( oldName != name )
    {
        emit nameChanged( name );
        oldName = name;
    }
}

int InputManager::playingStatus()
{
    return i_old_playing_status;
}

bool InputManager::hasAudio()
{
    if( hasInput() )
    {
        vlc_value_t val;
        var_Change( p_input, "audio-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
        return val.i_int > 0;
    }
    return false;
}

bool InputManager::hasVisualisation()
{
    if( !p_input )
        return false;

    audio_output_t *aout = input_GetAout( p_input );
    if( !aout )
        return false;

    char *visual = var_InheritString( aout, "visual" );
    vlc_object_release( aout );

    if( !visual )
        return false;

    free( visual );
    return true;
}

void InputManager::UpdateTeletext()
{
    if( hasInput() )
    {
        const bool b_enabled = var_CountChoices( p_input, "teletext-es" ) > 0;
        const int i_teletext_es = var_GetInteger( p_input, "teletext-es" );

        /* Teletext is possible. Show the buttons */
        emit teletextPossible( b_enabled );

        /* If Teletext is selected */
        if( b_enabled && i_teletext_es >= 0 )
        {
            /* Then, find the current page */
            int i_page = 100;
            bool b_transparent = false;

            if( p_input_vbi )
            {
                var_DelCallback( p_input_vbi, "vbi-page", VbiEvent, this );
                vlc_object_release( p_input_vbi );
            }

            if( input_GetEsObjects( p_input, i_teletext_es, &p_input_vbi, NULL, NULL ) )
                p_input_vbi = NULL;

            if( p_input_vbi )
            {
                /* This callback is not remove explicitly, but interfaces
                 * are guaranted to outlive input */
                var_AddCallback( p_input_vbi, "vbi-page", VbiEvent, this );

                i_page = var_GetInteger( p_input_vbi, "vbi-page" );
                b_transparent = !var_GetBool( p_input_vbi, "vbi-opaque" );
            }
            emit newTelexPageSet( i_page );
            emit teletextTransparencyActivated( b_transparent );

        }
        emit teletextActivated( b_enabled && i_teletext_es >= 0 );
    }
    else
    {
        emit teletextActivated( false );
        emit teletextPossible( false );
    }
}

void InputManager::UpdateEPG()
{
    if( hasInput() )
    {
       emit epgChanged();
    }
}

void InputManager::UpdateVout()
{
    if( hasInput() )
    {
        /* Get current vout lists from input */
        size_t i_vout;
        vout_thread_t **pp_vout;
        if( input_Control( p_input, INPUT_GET_VOUTS, &pp_vout, &i_vout ) )
        {
            i_vout = 0;
            pp_vout = NULL;
        }

        /* */
        emit voutListChanged( pp_vout, i_vout );

        /* */
        bool b_old_video = b_video;
        b_video = i_vout > 0;
        if( !!b_old_video != !!b_video )
            emit voutChanged( b_video );

        /* Release the vout list */
        for( size_t i = 0; i < i_vout; i++ )
            vlc_object_release( (vlc_object_t*)pp_vout[i] );
        free( pp_vout );
    }
}
void InputManager::UpdateAout()
{
    if( hasInput() )
    {
        /* TODO */
    }
}
void InputManager::UpdateCaching()
{
    if(!hasInput()) return;

    float f_newCache = var_GetFloat ( p_input, "cache" );
    if( f_newCache != f_cache )
    {
        f_cache = f_newCache;
        /* Update cache */
        emit cachingChanged( f_cache );
    }
}

void InputManager::requestArtUpdate()
{
    if( hasInput() )
    {
        playlist_AskForArtEnqueue( pl_Get(p_intf), input_GetItem( p_input ) );
    }
    else
    {
        /* No input will signal the cover art to update,
         * let's do it ourself */
        UpdateArt();
    }
}

const QString InputManager::decodeArtURL( input_item_t *p_item )
{
    assert( p_item );

    char *psz_art = input_item_GetArtURL( p_item );
    if( psz_art )
    {
        char *psz = make_path( psz_art );
        free( psz_art );
        psz_art = psz;
    }

#if 0
    /* Taglib seems to define a attachment://, It won't work yet */
    url = url.replace( "attachment://", "" );
#endif

    QString path = qfu( psz_art ? psz_art : "" );
    free( psz_art );
    return path;
}

void InputManager::UpdateArt()
{
    QString url;

    if( hasInput() )
        url = decodeArtURL( input_GetItem( p_input ) );

    /* the art hasn't changed, no need to update */
    if(artUrl == url)
        return;

    /* Update Art meta */
    artUrl = url;
    emit artChanged( artUrl );
}

inline void InputManager::UpdateStats()
{
    emit statisticsUpdated( input_GetItem( p_input ) );
}

inline void InputManager::UpdateMeta( input_item_t *p_item )
{
    emit metaChanged( p_item );
}

inline void InputManager::UpdateMeta()
{
    emit currentMetaChanged( input_GetItem( p_input ) );
}

inline void InputManager::UpdateInfo()
{
    emit infoChanged( input_GetItem( p_input ) );
}

void InputManager::UpdateRecord()
{
    if( hasInput() )
    {
        emit recordingStateChanged( var_GetBool( p_input, "record" ) );
    }
}

void InputManager::UpdateProgramEvent()
{
    if( hasInput() )
    {
        bool b_scrambled = var_GetBool( p_input, "program-scrambled" );
        emit encryptionChanged( b_scrambled );
    }
}

/* User update of the slider */
void InputManager::sliderUpdate( float new_pos )
{
    if( hasInput() )
        var_SetFloat( p_input, "position", new_pos );
    emit seekRequested( new_pos );
}

/* User togglePlayPause */
void InputManager::togglePlayPause()
{
    if( hasInput() )
    {
        int state = var_GetInteger( p_input, "state" );
        state = ( state != PLAYING_S ) ? PLAYING_S : PAUSE_S;
        var_SetInteger( p_input, "state", state );
    }
}

void InputManager::sectionPrev()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        var_TriggerCallback( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                             "prev-chapter":"prev-title" );
    }
}

void InputManager::sectionNext()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        var_TriggerCallback( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                             "next-chapter":"next-title" );
    }
}

void InputManager::sectionMenu()
{
    if( hasInput() )
    {
        vlc_value_t val, text;

        if( var_Change( p_input, "title  0", VLC_VAR_GETLIST, &val, &text ) < 0 )
            return;

        /* XXX is it "Root" or "Title" we want here ?" (set 0 by default) */
        int root = 0;
        for( int i = 0; i < val.p_list->i_count; i++ )
        {
            if( !strcmp( text.p_list->p_values[i].psz_string, "Title" ) )
                root = i;
        }
        var_FreeList( &val, &text );

        var_SetInteger( p_input, "title  0", root );
    }
}

/*
 *  Teletext Functions
 */

/* Set a new Teletext Page */
void InputManager::telexSetPage( int page )
{
    if( hasInput() && p_input_vbi )
    {
        const int i_teletext_es = var_GetInteger( p_input, "teletext-es" );

        if( i_teletext_es >= 0 )
        {
            var_SetInteger( p_input_vbi, "vbi-page", page );
            emit newTelexPageSet( page );
        }
    }
}

/* Set the transparency on teletext */
void InputManager::telexSetTransparency( bool b_transparentTelextext )
{
    if( hasInput() && p_input_vbi )
    {
        var_SetBool( p_input_vbi, "vbi-opaque", !b_transparentTelextext );
        emit teletextTransparencyActivated( b_transparentTelextext );
    }
}

void InputManager::activateTeletext( bool b_enable )
{
    vlc_value_t list;
    vlc_value_t text;
    if( hasInput() && !var_Change( p_input, "teletext-es", VLC_VAR_GETLIST, &list, &text ) )
    {
        if( list.p_list->i_count > 0 )
        {
            /* Prefer the page 100 if it is present */
            int i;
            for( i = 0; i < text.p_list->i_count; i++ )
            {
                /* The description is the page number as a string */
                const char *psz_page = text.p_list->p_values[i].psz_string;
                if( psz_page && !strcmp( psz_page, "100" ) )
                    break;
            }
            if( i >= list.p_list->i_count )
                i = 0;
            var_SetInteger( p_input, "spu-es", b_enable ? list.p_list->p_values[i].i_int : -1 );
        }
        var_FreeList( &list, &text );
    }
}

void InputManager::reverse()
{
    if( hasInput() )
    {
        float f_rate = var_GetFloat( p_input, "rate" );
        var_SetFloat( p_input, "rate", -f_rate );
    }
}

void InputManager::slower()
{
    var_TriggerCallback( THEPL, "rate-slower" );
}

void InputManager::faster()
{
    var_TriggerCallback( THEPL, "rate-faster" );
}

void InputManager::littlefaster()
{
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_RATE_FASTER_FINE );
}

void InputManager::littleslower()
{
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_RATE_SLOWER_FINE );
}

void InputManager::normalRate()
{
    var_SetFloat( THEPL, "rate", 1. );
}

void InputManager::setRate( int new_rate )
{
    var_SetFloat( THEPL, "rate",
                 (float)INPUT_RATE_DEFAULT / (float)new_rate );
}

void InputManager::jumpFwd()
{
    int i_interval = var_InheritInteger( p_input, "short-jump-size" );
    if( i_interval > 0 && hasInput() )
    {
        mtime_t val = CLOCK_FREQ * i_interval;
        var_SetTime( p_input, "time-offset", val );
    }
}

void InputManager::jumpBwd()
{
    int i_interval = var_InheritInteger( p_input, "short-jump-size" );
    if( i_interval > 0 && hasInput() )
    {
        mtime_t val = -CLOCK_FREQ * i_interval;
        var_SetTime( p_input, "time-offset", val );
    }
}

void InputManager::setAtoB()
{
    if( !timeA )
    {
        timeA = var_GetTime( THEMIM->getInput(), "time"  );
    }
    else if( !timeB )
    {
        timeB = var_GetTime( THEMIM->getInput(), "time"  );
        var_SetTime( THEMIM->getInput(), "time" , timeA );
        CONNECT( this, positionUpdated( float, int64_t, int ),
                 this, AtoBLoop( float, int64_t, int ) );
    }
    else
    {
        timeA = 0;
        timeB = 0;
        disconnect( this, SIGNAL( positionUpdated( float, int64_t, int ) ),
                    this, SLOT( AtoBLoop( float, int64_t, int ) ) );
    }
    emit AtoBchanged( (timeA != 0 ), (timeB != 0 ) );
}

/* Function called regularly when in an AtoB loop */
void InputManager::AtoBLoop( float, int64_t i_time, int )
{
    if( timeB )
    {
        if( i_time >= timeB || i_time < timeA )
            var_SetTime( THEMIM->getInput(), "time" , timeA );
    }
}

/**********************************************************************
 * MainInputManager implementation. Wrap an input manager and
 * take care of updating the main playlist input.
 * Used in the main playlist Dialog
 **********************************************************************/

MainInputManager::MainInputManager( intf_thread_t *_p_intf )
                 : QObject(NULL), p_intf( _p_intf )
{
    p_input = NULL;
    im = new InputManager( this, p_intf );

    var_AddCallback( THEPL, "item-change", ItemChanged, im );
    var_AddCallback( THEPL, "item-current", PLItemChanged, this );
    var_AddCallback( THEPL, "activity", PLItemChanged, this );
    var_AddCallback( THEPL, "leaf-to-parent", LeafToParent, this );
    var_AddCallback( THEPL, "playlist-item-append", PLItemAppended, this );
    var_AddCallback( THEPL, "playlist-item-deleted", PLItemRemoved, this );
    var_AddCallback( THEPL, "random", RandomChanged, this );
    var_AddCallback( THEPL, "repeat", RepeatChanged, this );
    var_AddCallback( THEPL, "loop", LoopChanged, this );

    var_AddCallback( THEPL, "volume", VolumeChanged, this );
    var_AddCallback( THEPL, "mute", SoundMuteChanged, this );

    /* Warn our embedded IM about input changes */
    DCONNECT( this, inputChanged( input_thread_t * ),
              im, setInput( input_thread_t * ) );

}

MainInputManager::~MainInputManager()
{
    if( p_input )
    {
       emit inputChanged( NULL );
       var_DelCallback( p_input, "state", PLItemChanged, this );
       vlc_object_release( p_input );
    }

    var_DelCallback( THEPL, "volume", VolumeChanged, this );
    var_DelCallback( THEPL, "mute", SoundMuteChanged, this );

    var_DelCallback( THEPL, "activity", PLItemChanged, this );
    var_DelCallback( THEPL, "item-change", ItemChanged, im );
    var_DelCallback( THEPL, "leaf-to-parent", LeafToParent, this );

    var_DelCallback( THEPL, "item-current", PLItemChanged, this );
    var_DelCallback( THEPL, "playlist-item-append", PLItemAppended, this );
    var_DelCallback( THEPL, "playlist-item-deleted", PLItemRemoved, this );
    var_DelCallback( THEPL, "random", RandomChanged, this );
    var_DelCallback( THEPL, "repeat", RepeatChanged, this );
    var_DelCallback( THEPL, "loop", LoopChanged, this );

    /* Save some interface state in configuration, at module quit */
    config_PutInt( p_intf, "random", var_GetBool( THEPL, "random" ) );
    config_PutInt( p_intf, "loop", var_GetBool( THEPL, "loop" ) );
    config_PutInt( p_intf, "repeat", var_GetBool( THEPL, "repeat" ) );

    if( var_InheritBool( p_intf, "qt-autosave-volume" ) )
        config_PutInt( p_intf, "volume", aout_VolumeGet( THEPL ) );
}

vout_thread_t* MainInputManager::getVout()
{
    return p_input ? input_GetVout( p_input ) : NULL;
}

audio_output_t * MainInputManager::getAout()
{
    return p_input ? input_GetAout( p_input ) : NULL;
}

void MainInputManager::customEvent( QEvent *event )
{
    int type = event->type();

    PLEvent *plEv;

    // msg_Dbg( p_intf, "New MainIM Event of type: %i", type );
    switch( type )
    {
    case VolumeChanged_Type:
        emit volumeChanged();
        return;
    case SoundMuteChanged_Type:
        emit soundMuteChanged();
        return;
    case PLItemAppended_Type:
        plEv = static_cast<PLEvent*>( event );
        emit playlistItemAppended( plEv->i_item, plEv->i_parent );
        return;
    case PLItemRemoved_Type:
        plEv = static_cast<PLEvent*>( event );
        emit playlistItemRemoved( plEv->i_item );
        return;
    case RandomChanged_Type:
        emit randomChanged( var_GetBool( THEPL, "random" ) );
        return;
    case LoopChanged_Type:
    case RepeatChanged_Type:
        notifyRepeatLoop();
        return;
    case LeafToParent_Type:
        plEv = static_cast<PLEvent*>( event );
        emit leafBecameParent( plEv->i_item );
        return;
    default:
        if( type != ItemChanged_Type ) return;
    }

    /* Should be PLItemChanged Event */
    if( !p_intf->p_sys->b_isDialogProvider )
    {
        if( p_input && ( p_input->b_dead || !vlc_object_alive (p_input) ) )
        {
            emit inputChanged( p_input );
            var_DelCallback( p_input, "state", PLItemChanged, this );
            vlc_object_release( p_input );
            p_input = NULL;
            return;
        }

        if( !p_input )
        {
            p_input = playlist_CurrentInput(THEPL);
            if( p_input )
            {
                var_AddCallback( p_input, "state", PLItemChanged, this );
                emit inputChanged( p_input );
            }
        }
    }
    else
    {
        /* remove previous stored p_input */
        if( p_input )
        {
            vlc_object_release( p_input );
            p_input = NULL;
        }
        /* we are working as a dialogs provider */
        p_input = playlist_CurrentInput( pl_Get(p_intf) );
        if( p_input )
        {
            emit inputChanged( p_input );
        }
    }
}

/* Playlist Control functions */
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

void MainInputManager::prevOrReset()
{
    if( !p_input || var_GetTime(  p_input , "time") < 10000 )
        playlist_Prev( THEPL );
    else
        getIM()->sliderUpdate( 0.0 );
}

void MainInputManager::togglePlayPause()
{
    /* No input, play */
    if( !p_input )
        playlist_Play( THEPL );
    else
        getIM()->togglePlayPause();
}

void MainInputManager::play()
{
    /* No input, play */
    if( !p_input )
        playlist_Play( THEPL );
    else
    {
        if( PLAYING_S != var_GetInteger( p_input, "state" ) )
        {
            getIM()->togglePlayPause();
        }
    }
}

void MainInputManager::pause()
{
    if(p_input && PLAYING_S == var_GetInteger( p_input, "state" ) )
    {
        getIM()->togglePlayPause();
    }
}

void MainInputManager::toggleRandom()
{
    var_ToggleBool( THEPL, "random" );
}

void MainInputManager::notifyRepeatLoop()
{
    int i_value = var_GetBool( THEPL, "loop" ) * REPEAT_ALL
              + var_GetBool( THEPL, "repeat" ) * REPEAT_ONE;

    emit repeatLoopChanged( i_value );
}

void MainInputManager::loopRepeatLoopStatus()
{
    /* Toggle Normal -> Loop -> Repeat -> Normal ... */
    if( var_GetBool( THEPL, "repeat" ) )
        var_SetBool( THEPL, "repeat", false );
    else if( var_GetBool( THEPL, "loop" ) )
    {
        var_SetBool( THEPL, "loop", false );
        var_SetBool( THEPL, "repeat", true );
    }
    else
        var_SetBool( THEPL, "loop", true );
}

void MainInputManager::activatePlayQuit( bool b_exit )
{
    var_SetBool( THEPL, "play-and-exit", b_exit );
}

bool MainInputManager::getPlayExitState()
{
    return var_GetBool( THEPL, "play-and-exit" );
}

/****************************
 * Static callbacks for MIM *
 ****************************/
static int PLItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );

    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( ItemChanged_Type );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

static int LeafToParent( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );
    MainInputManager *mim = (MainInputManager*)param;

    PLEvent *event = new PLEvent( LeafToParent_Type, newval.i_int );

    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

static int VolumeChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_var ); VLC_UNUSED( oldval ); VLC_UNUSED( newval );

    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( VolumeChanged_Type );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

static int SoundMuteChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_var ); VLC_UNUSED( oldval ); VLC_UNUSED( newval );

    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( SoundMuteChanged_Type );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

static int PLItemAppended
( vlc_object_t * obj, const char *var, vlc_value_t old, vlc_value_t cur, void *data )
{
    VLC_UNUSED( obj ); VLC_UNUSED( var ); VLC_UNUSED( old );

    MainInputManager *mim = static_cast<MainInputManager*>(data);
    playlist_add_t *p_add = static_cast<playlist_add_t*>( cur.p_address );

    PLEvent *event = new PLEvent( PLItemAppended_Type, p_add->i_item, p_add->i_node  );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}
static int PLItemRemoved
( vlc_object_t * obj, const char *var, vlc_value_t old, vlc_value_t cur, void *data )
{
    VLC_UNUSED( obj ); VLC_UNUSED( var ); VLC_UNUSED( old );

    MainInputManager *mim = static_cast<MainInputManager*>(data);

    PLEvent *event = new PLEvent( PLItemRemoved_Type, cur.i_int, 0  );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

static int RandomChanged
( vlc_object_t * obj, const char *var, vlc_value_t old, vlc_value_t cur, void *data )
{
    VLC_UNUSED( obj ); VLC_UNUSED( var ); VLC_UNUSED( old ); VLC_UNUSED( cur );

    MainInputManager *mim = static_cast<MainInputManager*>(data);

    IMEvent *event = new IMEvent( RandomChanged_Type );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

/* Probably could be merged with next callback */
static int LoopChanged
( vlc_object_t * obj, const char *var, vlc_value_t old, vlc_value_t cur, void *data )
{
    VLC_UNUSED( obj ); VLC_UNUSED( var ); VLC_UNUSED( old ); VLC_UNUSED( cur );

    MainInputManager *mim = static_cast<MainInputManager*>(data);

    IMEvent *event = new IMEvent( LoopChanged_Type );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

static int RepeatChanged
( vlc_object_t * obj, const char *var, vlc_value_t old, vlc_value_t cur, void *data )
{
    VLC_UNUSED( obj ); VLC_UNUSED( var ); VLC_UNUSED( old ); VLC_UNUSED( cur );

    MainInputManager *mim = static_cast<MainInputManager*>(data);

    IMEvent *event = new IMEvent( RepeatChanged_Type );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}
