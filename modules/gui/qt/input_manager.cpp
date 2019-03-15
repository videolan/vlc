/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "input_manager.hpp"
#include "recents.hpp"

#include <vlc_actions.h>           /* ACTION_ID */
#include <vlc_url.h>            /* vlc_uri_decode */
#include <vlc_strings.h>        /* vlc_strfinput */
#include <vlc_aout.h>           /* audio_output_t */

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QSignalMapper>
#include <QMessageBox>

#include <assert.h>

static int InputEvent( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );

/* Ensure arbitratry (not dynamically allocated) event IDs are not in use */
static inline void registerAndCheckEventIds( int start, int end )
{
    for ( int i=start ; i<=end ; i++ )
        Q_ASSERT( QEvent::registerEventType( i ) == i ); /* event ID collision ! */
}

/**********************************************************************
 * InputManager implementation
 **********************************************************************
 * The Input Manager can be the main one around the playlist
 * But can also be used for VLM dialog or similar
 **********************************************************************/

InputManager::InputManager( MainInputManager *mim, intf_thread_t *_p_intf) :
                           QObject( mim ), p_intf( _p_intf )
{
    p_mim        = mim;
    i_old_playing_status = END_S;
    oldName      = "";
    artUrl       = "";
    p_input      = NULL;
    f_rate       = 0.;
    p_item       = NULL;
    b_video      = false;
    timeA        = VLC_TICK_INVALID;
    timeB        = VLC_TICK_INVALID;
    f_cache      = -1.; /* impossible initial value, different from all */
    registerAndCheckEventIds( IMEvent::PositionUpdate, IMEvent::FullscreenControlPlanHide );
    registerAndCheckEventIds( PLEvent::PLItemAppended, PLEvent::PLEmpty );
}

InputManager::~InputManager()
{
    delInput();
}

void InputManager::inputChangedHandler()
{
    setInput( p_mim->getInput() );
}

/* Define the Input used.
   Add the callbacks on input
   p_input is held once here */
void InputManager::setInput( input_thread_t *_p_input )
{
    delInput();
    p_input = _p_input;
    if( p_input != NULL )
    {
        msg_Dbg( p_intf, "IM: Setting an input" );
        input_Hold(p_input);
        addCallbacks();

        UpdateStatus();
        UpdateName();
        UpdateArt();
        UpdateTeletext();
        UpdateNavigation();
        UpdateVout();

        p_item = input_GetItem( p_input );
        emit rateChanged( var_GetFloat( p_input, "rate" ) );

        /* Get Saved Time */
        if( p_item->i_type == ITEM_TYPE_FILE )
        {
            char *uri = input_item_GetURI( p_item );

            vlc_tick_t i_time = RecentsMRL::getInstance( p_intf )->time( qfu(uri) );
            if( i_time > 0 && qfu( uri ) != lastURI &&
                    !var_GetFloat( p_input, "run-time" ) &&
                    !var_GetFloat( p_input, "start-time" ) &&
                    !var_GetFloat( p_input, "stop-time" ) )
            {
                emit resumePlayback( i_time );
            }
            playlist_Lock( THEPL );
            // Add root items only
            playlist_item_t* p_node = playlist_CurrentPlayingItem( THEPL );
            if ( p_node != NULL && p_node->p_parent != NULL && p_node->p_parent->i_id == THEPL->p_playing->i_id )
            {
                // Save the latest URI to avoid asking to restore the
                // position on the same input file.
                lastURI = qfu( uri );
                RecentsMRL::getInstance( p_intf )->addRecent( lastURI );
            }
            playlist_Unlock( THEPL );
            free( uri );
        }
    }
    else
    {
        p_item = NULL;
        lastURI.clear();
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

    /* Save time / position */
    char *uri = input_item_GetURI( p_item );
    if( uri != NULL ) {
        float f_pos = var_GetFloat( p_input , "position" );
        vlc_tick_t i_time = -1;

        if( f_pos >= 0.05f && f_pos <= 0.95f
         && var_GetInteger( p_input, "length" ) >= VLC_TICK_FROM_SEC(60))
            i_time = var_GetInteger( p_input, "time");

        RecentsMRL::getInstance( p_intf )->setTime( qfu(uri), i_time );
        free(uri);
    }

    delCallbacks();
    i_old_playing_status = END_S;
    p_item               = NULL;
    oldName              = "";
    artUrl               = "";
    b_video              = false;
    timeA                = VLC_TICK_INVALID;
    timeB                = VLC_TICK_INVALID;
    f_rate               = 0. ;

    input_Release( p_input );
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
    emit artChanged( "" );
    emit infoChanged( NULL );
    emit currentMetaChanged( (input_item_t *)NULL );

    emit encryptionChanged( false );
    emit recordingStateChanged( false );

    emit cachingChanged( 0.0 );
}

/* Convert the event from the callbacks in actions */
void InputManager::customEvent( QEvent *event )
{
    int i_type = event->type();
    IMEvent *ple = static_cast<IMEvent *>(event);

    if( i_type == IMEvent::ItemChanged )
        UpdateMeta( ple->item() );

    if( !hasInput() )
        return;

    /* Actions */
    switch( i_type )
    {
    case IMEvent::CapabilitiesChanged:
        UpdateCapabilities();
        break;
    case IMEvent::PositionUpdate:
        UpdatePosition();
        break;
    case IMEvent::StatisticsUpdate:
        UpdateStats();
        break;
    case IMEvent::ItemChanged:
        /* Ignore ItemChanged_Type event that does not apply to our input */
        if( p_item == ple->item() )
        {
            UpdateStatus();
            UpdateName();
            UpdateArt();
            UpdateMeta();
            /* Update duration of file */
        }
        break;
    case IMEvent::ItemStateChanged:
        UpdateStatus();
        break;
    case IMEvent::MetaChanged:
        UpdateMeta();
        UpdateName(); /* Needed for NowPlaying */
        UpdateArt(); /* Art is part of meta in the core */
        break;
    case IMEvent::InfoChanged:
        UpdateInfo();
        break;
    case IMEvent::ItemTitleChanged:
        UpdateNavigation();
        UpdateName(); /* Display the name of the Chapter, if exists */
        break;
    case IMEvent::ItemRateChanged:
        UpdateRate();
        break;
    case IMEvent::ItemEsChanged:
        UpdateTeletext();
        break;
    case IMEvent::InterfaceVoutUpdate:
        UpdateVout();
        break;
    case IMEvent::SynchroChanged:
        emit synchroChanged();
        break;
    case IMEvent::CachingEvent:
        UpdateCaching();
        break;
    case IMEvent::BookmarksChanged:
        emit bookmarksChanged();
        break;
    case IMEvent::RecordingEvent:
        UpdateRecord();
        break;
    case IMEvent::ProgramChanged:
        UpdateProgramEvent();
        break;
    case IMEvent::EPGEvent:
        UpdateEPG();
        break;
    default:
        msg_Warn( p_intf, "This shouldn't happen: %i", i_type );
        vlc_assert_unreachable();
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
int MainInputManager::ItemChanged( vlc_object_t *, const char *,
                                   vlc_value_t, vlc_value_t val, void *param )
{
    InputManager *im = (InputManager*)param;
    input_item_t *p_item = static_cast<input_item_t *>(val.p_address);

    IMEvent *event = new IMEvent( IMEvent::ItemChanged, p_item );
    QApplication::postEvent( im, event );
    return VLC_SUCCESS;
}

static int InputEvent( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;
    IMEvent *event;

    switch( newval.i_int )
    {
    case INPUT_EVENT_STATE:
        event = new IMEvent( IMEvent::ItemStateChanged );
        break;
    case INPUT_EVENT_RATE:
        event = new IMEvent( IMEvent::ItemRateChanged );
        break;
    case INPUT_EVENT_CAPABILITIES:
        event = new IMEvent( IMEvent::CapabilitiesChanged );
        break;
    case INPUT_EVENT_POSITION:
    //case INPUT_EVENT_LENGTH:
        event = new IMEvent( IMEvent::PositionUpdate );
        break;

    case INPUT_EVENT_TITLE:
    case INPUT_EVENT_CHAPTER:
        event = new IMEvent( IMEvent::ItemTitleChanged );
        break;

    case INPUT_EVENT_ES:
    case INPUT_EVENT_VBI_PAGE:
        event = new IMEvent( IMEvent::ItemEsChanged );
        break;

    case INPUT_EVENT_STATISTICS:
        event = new IMEvent( IMEvent::StatisticsUpdate );
        break;

    case INPUT_EVENT_VOUT:
        event = new IMEvent( IMEvent::InterfaceVoutUpdate );
        break;

    case INPUT_EVENT_ITEM_META: /* Codec MetaData + Art */
        event = new IMEvent( IMEvent::MetaChanged );
        break;
    case INPUT_EVENT_ITEM_INFO: /* Codec Info */
        event = new IMEvent( IMEvent::InfoChanged );
        break;

    case INPUT_EVENT_AUDIO_DELAY:
    case INPUT_EVENT_SUBTITLE_DELAY:
        event = new IMEvent( IMEvent::SynchroChanged );
        break;

    case INPUT_EVENT_CACHE:
        event = new IMEvent( IMEvent::CachingEvent );
        break;

    case INPUT_EVENT_BOOKMARK:
        event = new IMEvent( IMEvent::BookmarksChanged );
        break;

    case INPUT_EVENT_RECORD:
        event = new IMEvent( IMEvent::RecordingEvent );
        break;

    case INPUT_EVENT_PROGRAM:
        /* This is for PID changes */
        event = new IMEvent( IMEvent::ProgramChanged );
        break;

    case INPUT_EVENT_ITEM_EPG:
        /* EPG data changed */
        event = new IMEvent( IMEvent::EPGEvent );
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

void InputManager::UpdatePosition()
{
    /* Update position */
    vlc_tick_t i_length = var_GetInteger(  p_input , "length" );
    vlc_tick_t i_time = var_GetInteger(  p_input , "time");
    float f_pos = var_GetFloat(  p_input , "position" );
    emit positionUpdated( f_pos, i_time, SEC_FROM_VLC_TICK(i_length) );
}

void InputManager::UpdateNavigation()
{
    /* Update navigation status */
    size_t ntitles, nchapters;

    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &ntitles );

    if( ntitles > 0 )
    {
        bool b_menu = false;
        if( ntitles > 1 )
        {
            input_title_t **pp_title = NULL;
            int i_title = 0;
            if( input_Control( p_input, INPUT_GET_FULL_TITLE_INFO, &pp_title, &i_title ) == VLC_SUCCESS )
            {
                for( int i = 0; i < i_title; i++ )
                {
                    if( pp_title[i]->i_flags & INPUT_TITLE_MENU )
                        b_menu = true;
                    vlc_input_title_Delete(pp_title[i]);
                }
                free( pp_title );
            }
        }

        /* p_input != NULL since val.i_int != 0 */
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &nchapters );

        emit titleChanged( b_menu );
        emit chapterChanged( nchapters > 1 );
    }
    else
        emit chapterChanged( false );

    if( hasInput() )
        emit inputCanSeek( var_GetBool( p_input, "can-seek" ) );
    else
        emit inputCanSeek( false );
}

void InputManager::UpdateCapabilities()
{
    emit inputCanSeek( var_GetBool( p_input, "can-seek" ) );
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
    char *formatted = NULL;
    if (format != NULL)
    {
        formatted = vlc_strfinput( p_input, NULL, format );
        free( format );
        if( formatted != NULL )
        {
            name = qfu(formatted);
            free( formatted );
        }
    }

    /* If we have Nothing */
    if( name.simplified().isEmpty() )
    {
        char *uri = input_item_GetURI( input_GetItem( p_input ) );
        char *file = uri ? strrchr( uri, '/' ) : NULL;
        if( file != NULL )
        {
            vlc_uri_decode( ++file );
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

int InputManager::playingStatus() const
{
    return i_old_playing_status;
}

bool InputManager::hasAudio()
{
    if( hasInput() )
    {
        size_t val;
        var_Change( p_input, "audio-es", VLC_VAR_CHOICESCOUNT, &val );
        return val > 0;
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
    aout_Release( aout );

    if( !visual )
        return false;

    free( visual );
    return true;
}

void InputManager::UpdateTeletext()
{
    const bool b_enabled = var_CountChoices( p_input, "teletext-es" ) > 0;
    const int i_teletext_es = var_GetInteger( p_input, "teletext-es" );

    /* Teletext is possible. Show the buttons */
    emit teletextPossible( b_enabled );

    /* If Teletext is selected */
    if( b_enabled && i_teletext_es >= 0 )
    {
        vlc_object_t *p_input_vbi;
        /* Then, find the current page */
        int i_page = 100;
        bool b_transparent = false;

        if( input_GetEsObjects( p_input, i_teletext_es, &p_input_vbi ) )
            p_input_vbi = NULL;

        if( p_input_vbi )
        {
            i_page = var_GetInteger( p_input_vbi, "vbi-page" );
            b_transparent = !var_GetBool( p_input_vbi, "vbi-opaque" );
            vlc_object_release( p_input_vbi );
        }
        emit newTelexPageSet( i_page );
        emit teletextTransparencyActivated( b_transparent );

    }
    emit teletextActivated( b_enabled && i_teletext_es >= 0 );
}

void InputManager::UpdateEPG()
{
    emit epgChanged();
}

void InputManager::UpdateVout()
{
    size_t i_vout;
    vout_thread_t **pp_vout;

    if( !p_input )
        return;

    /* Get current vout lists from input */
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
        vout_Release(pp_vout[i]);
    free( pp_vout );
}

void InputManager::UpdateCaching()
{
    float f_newCache = var_GetFloat ( p_input, "cache" );
    if( f_newCache != f_cache )
    {
        f_cache = f_newCache;
        /* Update cache */
        emit cachingChanged( f_cache );
    }
}

void InputManager::requestArtUpdate( input_item_t *p_item, bool b_forced )
{
    bool b_current_item = false;
    if ( !p_item && hasInput() )
    {   /* default to current item */
        p_item = input_GetItem( p_input );
        b_current_item = true;
    }

    if ( p_item )
    {
        /* check if it has already been enqueued */
        if ( p_item->p_meta && !b_forced )
        {
            int status = vlc_meta_GetStatus( p_item->p_meta );
            if ( status & ( ITEM_ART_NOTFOUND|ITEM_ART_FETCHED ) )
                return;
        }
        libvlc_ArtRequest( vlc_object_instance(p_intf), p_item,
                           (b_forced) ? META_REQUEST_OPTION_SCOPE_ANY
                                      : META_REQUEST_OPTION_NONE,
                           NULL, NULL );
        /* No input will signal the cover art to update,
             * let's do it ourself */
        if ( b_current_item )
            UpdateArt();
        else
            emit artChanged( p_item );
    }
}

const QString InputManager::decodeArtURL( input_item_t *p_item )
{
    assert( p_item );

    char *psz_art = input_item_GetArtURL( p_item );
    if( psz_art )
    {
        char *psz = vlc_uri2path( psz_art );
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
    QString url = decodeArtURL( input_GetItem( p_input ) );

    /* the art hasn't changed, no need to update */
    if(artUrl == url)
        return;

    /* Update Art meta */
    artUrl = url;
    emit artChanged( artUrl );
}

void InputManager::setArt( input_item_t *p_item, QString fileUrl )
{
    if( hasInput() )
    {
        char *psz_cachedir = config_GetUserDir( VLC_CACHE_DIR );
        QString old_url = p_mim->getIM()->decodeArtURL( p_item );
        old_url = QDir( old_url ).canonicalPath();

        if( old_url.startsWith( QString::fromUtf8( psz_cachedir ) ) )
            QFile( old_url ).remove(); /* Purge cached artwork */

        free( psz_cachedir );

        input_item_SetArtURL( p_item , fileUrl.toUtf8().constData() );
        UpdateArt();
    }
}

inline void InputManager::UpdateStats()
{
    emit statisticsUpdated( input_GetItem( p_input ) );
}

inline void InputManager::UpdateMeta( input_item_t *p_item_ )
{
    emit metaChanged( p_item_ );
    emit artChanged( p_item_ );
}

inline void InputManager::UpdateMeta()
{
    emit currentMetaChanged( input_GetItem( p_input ) );
}

inline void InputManager::UpdateInfo()
{
    assert( p_input );
    emit infoChanged( input_GetItem( p_input ) );
}

void InputManager::UpdateRecord()
{
    emit recordingStateChanged( var_GetBool( p_input, "record" ) );
}

void InputManager::UpdateProgramEvent()
{
    bool b_scrambled = var_GetBool( p_input, "program-scrambled" );
    emit encryptionChanged( b_scrambled );
}

/* User update of the slider */
void InputManager::sliderUpdate( float new_pos )
{
    if( hasInput() )
        var_SetFloat( p_input, "position", new_pos );
    emit seekRequested( new_pos );
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
        var_TriggerCallback( p_input, "menu-title" );
    }
}

/*
 *  Teletext Functions
 */

void InputManager::changeProgram( int program )
{
    if( hasInput() )
    {
        var_SetInteger( p_input, "program", program );
    }
}

/* Set a new Teletext Page */
void InputManager::telexSetPage( int page )
{
    vlc_object_t *input_vbi;

    if (!hasInput())
        return;

    const int i_teletext_es = var_GetInteger(p_input, "teletext-es");
    if (i_teletext_es >= 0)
    {
        if (input_GetEsObjects(p_input, i_teletext_es, &input_vbi)
         || input_vbi == NULL)
            return;

        var_SetInteger(input_vbi, "vbi-page", page);
        vlc_object_release(input_vbi);
        emit newTelexPageSet(page);
    }
}

/* Set the transparency on teletext */
void InputManager::telexSetTransparency( bool b_transparentTelextext )
{
    vlc_object_t *input_vbi;

    if (!hasInput())
        return;

    const int i_teletext_es = var_GetInteger(p_input, "teletext-es");
    if (i_teletext_es >= 0)
    {
        if (input_GetEsObjects(p_input, i_teletext_es, &input_vbi)
         || input_vbi == NULL)
            return;

        var_SetBool(input_vbi, "vbi-opaque", !b_transparentTelextext);
        vlc_object_release(input_vbi);
        emit teletextTransparencyActivated( b_transparentTelextext );
    }
}

void InputManager::activateTeletext( bool b_enable )
{
    vlc_value_t *list;
    char **text;
    size_t count;

    if( hasInput() && !var_Change( p_input, "teletext-es", VLC_VAR_GETCHOICES,
                                   &count, &list, &text ) )
    {
        if( count > 0 )
        {   /* Prefer the page 100 if it is present */
            int id = list[0].i_int;
            for( size_t i = 0; i < count; i++ )
            {   /* The description is the page number as a string */
                if( text[i] != NULL && !strcmp( text[i], "100" ) )
                    id = list[i].i_int;
                free(text[i]);
            }
            var_SetInteger( p_input, "spu-es", b_enable ? id : -1 );
        }
        free(text);
        free(list);
    }
}

void InputManager::reverse()
{
    if( hasInput() )
    {
        float f_rate_ = var_GetFloat( p_input, "rate" );
        var_SetFloat( p_input, "rate", -f_rate_ );
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
    var_SetInteger( vlc_object_instance(p_intf), "key-action", ACTIONID_RATE_FASTER_FINE );
}

void InputManager::littleslower()
{
    var_SetInteger( vlc_object_instance(p_intf), "key-action", ACTIONID_RATE_SLOWER_FINE );
}

void InputManager::normalRate()
{
    var_SetFloat( THEPL, "rate", 1. );
}

void InputManager::setRate( float new_rate )
{
    var_SetFloat( THEPL, "rate", new_rate );
}

void InputManager::jumpFwd()
{
    int i_interval = var_InheritInteger( p_input, "short-jump-size" );
    if( i_interval > 0 && hasInput() )
    {
        vlc_tick_t val = vlc_tick_from_sec( i_interval );
        var_SetInteger( p_input, "time-offset", val );
    }
}

void InputManager::jumpBwd()
{
    int i_interval = var_InheritInteger( p_input, "short-jump-size" );
    if( i_interval > 0 && hasInput() )
    {
        vlc_tick_t val = vlc_tick_from_sec( -i_interval );
        var_SetInteger( p_input, "time-offset", val );
    }
}

void InputManager::setAtoB()
{
    if( timeA != VLC_TICK_INVALID )
    {
        timeA = var_GetInteger( p_mim->getInput(), "time"  );
    }
    else if( timeB != VLC_TICK_INVALID )
    {
        timeB = var_GetInteger( p_mim->getInput(), "time"  );
        var_SetInteger( p_mim->getInput(), "time" , timeA );
        CONNECT( this, positionUpdated( float, vlc_tick_t, int ),
                 this, AtoBLoop( float, vlc_tick_t, int ) );
    }
    else
    {
        timeA = VLC_TICK_INVALID;
        timeB = VLC_TICK_INVALID;
        disconnect( this, SIGNAL( positionUpdated( float, vlc_tick_t, int ) ),
                    this, SLOT( AtoBLoop( float, vlc_tick_t, int ) ) );
    }
    emit AtoBchanged( (timeA != VLC_TICK_INVALID ), (timeB != VLC_TICK_INVALID ) );
}

/* Function called regularly when in an AtoB loop */
void InputManager::AtoBLoop( float, vlc_tick_t i_time, int )
{
    if( timeB != VLC_TICK_INVALID && i_time >= timeB )
        var_SetInteger( p_mim->getInput(), "time" , timeA );
}

/**********************************************************************
 * MainInputManager implementation. Wrap an input manager and
 * take care of updating the main playlist input.
 * Used in the main playlist Dialog
 **********************************************************************/

MainInputManager::MainInputManager( intf_thread_t *_p_intf )
    : QObject(NULL), p_input( NULL), p_intf( _p_intf ),
      random( VLC_OBJECT(THEPL), "random" ),
      repeat( VLC_OBJECT(THEPL), "repeat" ), loop( VLC_OBJECT(THEPL), "loop" ),
      volume( VLC_OBJECT(THEPL), "volume" ), mute( VLC_OBJECT(THEPL), "mute" )
{
    im = new InputManager( this, p_intf );

    /* Audio Menu */
    menusAudioMapper = new QSignalMapper();
    CONNECT( menusAudioMapper, mapped(QString), this, menusUpdateAudio( QString ) );

    /* Core Callbacks */
    var_AddCallback( THEPL, "item-change", MainInputManager::ItemChanged, im );
    var_AddCallback( THEPL, "input-current", MainInputManager::PLItemChanged, this );
    var_AddCallback( THEPL, "leaf-to-parent", MainInputManager::LeafToParent, this );
    var_AddCallback( THEPL, "playlist-item-append", MainInputManager::PLItemAppended, this );
    var_AddCallback( THEPL, "playlist-item-deleted", MainInputManager::PLItemRemoved, this );

    /* Core Callbacks to widget */
    random.addCallback( this, SLOT(notifyRandom(bool)) );
    repeat.addCallback( this, SLOT(notifyRepeatLoop(bool)) );
    loop.addCallback(   this, SLOT(notifyRepeatLoop(bool)) );
    volume.addCallback( this, SLOT(notifyVolume(float)) );
    mute.addCallback(   this, SLOT(notifyMute(bool)) );

    /* Warn our embedded IM about input changes */
    DCONNECT( this, inputChanged( bool ),
              im, inputChangedHandler() );
}

MainInputManager::~MainInputManager()
{
    if( p_input )
    {
       input_Release(p_input);
       p_input = NULL;
       emit inputChanged( false );
    }

    var_DelCallback( THEPL, "input-current", MainInputManager::PLItemChanged, this );
    var_DelCallback( THEPL, "item-change", MainInputManager::ItemChanged, im );
    var_DelCallback( THEPL, "leaf-to-parent", MainInputManager::LeafToParent, this );

    var_DelCallback( THEPL, "playlist-item-append", MainInputManager::PLItemAppended, this );
    var_DelCallback( THEPL, "playlist-item-deleted", MainInputManager::PLItemRemoved, this );

    delete menusAudioMapper;
}

vout_thread_t* MainInputManager::getVout()
{
    return p_input ? input_GetVout( p_input ) : NULL;
}

QVector<vout_thread_t*> MainInputManager::getVouts() const
{
    vout_thread_t **pp_vout;
    size_t i_vout;

    if( p_input == NULL
     || input_Control( p_input, INPUT_GET_VOUTS, &pp_vout, &i_vout ) != VLC_SUCCESS
     || i_vout == 0 )
        return QVector<vout_thread_t*>();

    QVector<vout_thread_t*> vector = QVector<vout_thread_t*>();
    vector.reserve( i_vout );
    for( size_t i = 0; i < i_vout; i++ )
    {
        assert( pp_vout[i] );
        vector.append( pp_vout[i] );
    }
    free( pp_vout );

    return vector;
}

audio_output_t * MainInputManager::getAout()
{
    return playlist_GetAout( THEPL );
}

void MainInputManager::customEvent( QEvent *event )
{
    int type = event->type();

    PLEvent *plEv;

    // msg_Dbg( p_intf, "New MainIM Event of type: %i", type );
    switch( type )
    {
    case PLEvent::PLItemAppended:
        plEv = static_cast<PLEvent*>( event );
        emit playlistItemAppended( plEv->getItemId(), plEv->getParentId() );
        return;
    case PLEvent::PLItemRemoved:
        plEv = static_cast<PLEvent*>( event );
        emit playlistItemRemoved( plEv->getItemId() );
        return;
    case PLEvent::PLEmpty:
        plEv = static_cast<PLEvent*>( event );
        emit playlistNotEmpty( plEv->getItemId() >= 0 );
        return;
    case PLEvent::LeafToParent:
        plEv = static_cast<PLEvent*>( event );
        emit leafBecameParent( plEv->getItemId() );
        return;
    default:
        if( type != IMEvent::ItemChanged ) return;
    }
    probeCurrentInput();
}

void MainInputManager::probeCurrentInput()
{
    if( p_input != NULL )
        input_Release(p_input);
    p_input = playlist_CurrentInput( THEPL );
    emit inputChanged( p_input != NULL );
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
    if( !p_input || var_GetInteger( p_input, "time") < VLC_TICK_FROM_MS(10) )
        playlist_Prev( THEPL );
    else
        getIM()->sliderUpdate( 0.0 );
}

void MainInputManager::togglePlayPause()
{
    playlist_TogglePause( THEPL );
}

void MainInputManager::play()
{
    playlist_Play( THEPL );
}

void MainInputManager::pause()
{
    playlist_Pause( THEPL );
}

void MainInputManager::toggleRandom()
{
    config_PutInt( "random", var_ToggleBool( THEPL, "random" ) );
}

void MainInputManager::notifyRandom(bool value)
{
    emit randomChanged(value);
}

void MainInputManager::notifyRepeatLoop(bool)
{
    int i_state = NORMAL;

    if( var_GetBool( THEPL, "loop" ) )   i_state = REPEAT_ALL;
    if( var_GetBool( THEPL, "repeat" ) ) i_state = REPEAT_ONE;

    emit repeatLoopChanged( i_state );
}

void MainInputManager::loopRepeatLoopStatus()
{
    /* Toggle Normal -> Loop -> Repeat -> Normal ... */
    bool loop = var_GetBool( THEPL, "loop" );
    bool repeat = var_GetBool( THEPL, "repeat" );

    if( repeat )
    {
        loop = false;
        repeat = false;
    }
    else if( loop )
    {
        loop = false;
        repeat = true;
    }
    else
    {
        loop = true;
        //repeat = false;
    }

    var_SetBool( THEPL, "loop", loop );
    var_SetBool( THEPL, "repeat", repeat );
    config_PutInt( "loop", loop );
    config_PutInt( "repeat", repeat );
}

void MainInputManager::activatePlayQuit( bool b_exit )
{
    var_SetBool( THEPL, "play-and-exit", b_exit );
    config_PutInt( "play-and-exit", b_exit );
}

bool MainInputManager::getPlayExitState()
{
    return var_InheritBool( THEPL, "play-and-exit" );
}

bool MainInputManager::hasEmptyPlaylist()
{
    playlist_Lock( THEPL );
    bool b_empty = playlist_IsEmpty( THEPL );
    playlist_Unlock( THEPL );
    return b_empty;
}

/****************************
 * Static callbacks for MIM *
 ****************************/
int MainInputManager::PLItemChanged( vlc_object_t *, const char *,
                                     vlc_value_t, vlc_value_t, void *param )
{
    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( IMEvent::ItemChanged );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

int MainInputManager::LeafToParent( vlc_object_t *, const char *,
                                    vlc_value_t, vlc_value_t val, void *param )
{
    MainInputManager *mim = (MainInputManager*)param;

    PLEvent *event = new PLEvent( PLEvent::LeafToParent, val.i_int );

    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

void MainInputManager::notifyVolume( float volume )
{
    emit volumeChanged( volume );
}

void MainInputManager::notifyMute( bool mute )
{
    emit soundMuteChanged(mute);
}


void MainInputManager::menusUpdateAudio( const QString& data )
{
    audio_output_t *aout = getAout();
    if( aout != NULL )
    {
        aout_DeviceSet( aout, qtu(data) );
        aout_Release(aout);
    }
}

int MainInputManager::PLItemAppended( vlc_object_t *, const char *,
                                      vlc_value_t, vlc_value_t cur,
                                      void *data )
{
    MainInputManager *mim = static_cast<MainInputManager*>(data);
    playlist_item_t *item = static_cast<playlist_item_t *>( cur.p_address );

    PLEvent *event = new PLEvent( PLEvent::PLItemAppended, item->i_id,
        (item->p_parent != NULL) ? item->p_parent->i_id : -1  );
    QApplication::postEvent( mim, event );

    event = new PLEvent( PLEvent::PLEmpty, item->i_id, 0  );
    QApplication::postEvent( mim, event );
    return VLC_SUCCESS;
}

int MainInputManager::PLItemRemoved( vlc_object_t *obj, const char *,
                                     vlc_value_t, vlc_value_t cur, void *data )
{
    playlist_t *pl = (playlist_t *) obj;
    MainInputManager *mim = static_cast<MainInputManager*>(data);
    playlist_item_t *item = static_cast<playlist_item_t *>( cur.p_address );

    PLEvent *event = new PLEvent( PLEvent::PLItemRemoved, item->i_id, 0  );
    QApplication::postEvent( mim, event );
    // can't use playlist_IsEmpty(  ) as it isn't true yet
    if ( pl->items.i_size == 1 ) // lock is held
    {
        event = new PLEvent( PLEvent::PLEmpty, -1, 0 );
        QApplication::postEvent( mim, event );
    }
    return VLC_SUCCESS;
}

void MainInputManager::changeFullscreen( bool new_val )
{
    if ( var_GetBool( THEPL, "fullscreen" ) != new_val)
        var_SetBool( THEPL, "fullscreen", new_val );
}
