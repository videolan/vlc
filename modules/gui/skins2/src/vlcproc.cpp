/*****************************************************************************
 * vlcproc.cpp
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          Erwan Tulou      <erwan10@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_playlist.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include "vlcproc.hpp"
#include "os_factory.hpp"
#include "os_loop.hpp"
#include "os_timer.hpp"
#include "var_manager.hpp"
#include "vout_manager.hpp"
#include "fsc_window.hpp"
#include "theme.hpp"
#include "window_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_change_skin.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_resize.hpp"
#include "../commands/cmd_vars.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_audio.hpp"
#include "../commands/cmd_callbacks.hpp"
#include "../utils/var_bool.hpp"
#include "../utils/var_string.hpp"
#include <sstream>

#include <assert.h>

VlcProc *VlcProc::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_vlcProc == NULL )
    {
        pIntf->p_sys->p_vlcProc = new VlcProc( pIntf );
    }

    return pIntf->p_sys->p_vlcProc;
}


void VlcProc::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_vlcProc;
    pIntf->p_sys->p_vlcProc = NULL;
}

#define SET_BOOL(m,v)         ((VarBoolImpl*)(m).get())->set(v)
#define SET_STREAMTIME(m,v,b) ((StreamTime*)(m).get())->set(v,b)
#define SET_TEXT(m,v)         ((VarText*)(m).get())->set(v)
#define SET_STRING(m,v)       ((VarString*)(m).get())->set(v)
#define SET_VOLUME(m,v,b)     ((Volume*)(m).get())->setVolume(v,b)

VlcProc::VlcProc( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_varEqBands( pIntf ), m_pVout( NULL )
{
    // Create and register VLC variables
    VarManager *pVarManager = VarManager::instance( getIntf() );

#define REGISTER_VAR( var, type, name ) \
    var = VariablePtr( new type( getIntf() ) ); \
    pVarManager->registerVar( var, name );
    REGISTER_VAR( m_cVarRandom, VarBoolImpl, "playlist.isRandom" )
    REGISTER_VAR( m_cVarLoop, VarBoolImpl, "playlist.isLoop" )
    REGISTER_VAR( m_cVarRepeat, VarBoolImpl, "playlist.isRepeat" )
    REGISTER_VAR( m_cPlaytree, Playtree, "playtree" )
    pVarManager->registerVar( getPlaytreeVar().getPositionVarPtr(),
                              "playtree.slider" );
    pVarManager->registerVar( m_cVarRandom, "playtree.isRandom" );
    pVarManager->registerVar( m_cVarLoop, "playtree.isLoop" );

    REGISTER_VAR( m_cVarPlaying, VarBoolImpl, "vlc.isPlaying" )
    REGISTER_VAR( m_cVarStopped, VarBoolImpl, "vlc.isStopped" )
    REGISTER_VAR( m_cVarPaused, VarBoolImpl, "vlc.isPaused" )

    /* Input variables */
    pVarManager->registerVar( m_cVarRepeat, "playtree.isRepeat" );
    REGISTER_VAR( m_cVarTime, StreamTime, "time" )
    REGISTER_VAR( m_cVarSeekable, VarBoolImpl, "vlc.isSeekable" )
    REGISTER_VAR( m_cVarDvdActive, VarBoolImpl, "dvd.isActive" )

    REGISTER_VAR( m_cVarRecordable, VarBoolImpl, "vlc.canRecord" )
    REGISTER_VAR( m_cVarRecording, VarBoolImpl, "vlc.isRecording" )

    /* Vout variables */
    REGISTER_VAR( m_cVarFullscreen, VarBoolImpl, "vlc.isFullscreen" )
    REGISTER_VAR( m_cVarHasVout, VarBoolImpl, "vlc.hasVout" )

    /* Aout variables */
    REGISTER_VAR( m_cVarHasAudio, VarBoolImpl, "vlc.hasAudio" )
    REGISTER_VAR( m_cVarVolume, Volume, "volume" )
    REGISTER_VAR( m_cVarMute, VarBoolImpl, "vlc.isMute" )
    REGISTER_VAR( m_cVarEqualizer, VarBoolImpl, "equalizer.isEnabled" )
    REGISTER_VAR( m_cVarEqPreamp, EqualizerPreamp, "equalizer.preamp" )

#undef REGISTER_VAR
    m_cVarSpeed = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarSpeed, "speed" );
    SET_TEXT( m_cVarSpeed, UString( getIntf(), "1") );
    m_cVarStreamName = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamName, "streamName" );
    m_cVarStreamURI = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamURI, "streamURI" );
    m_cVarStreamBitRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamBitRate, "bitrate" );
    m_cVarStreamSampleRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamSampleRate, "samplerate" );
    m_cVarStreamArt = VariablePtr( new VarString( getIntf() ) );
    pVarManager->registerVar( m_cVarStreamArt, "streamArt" );

    // Register the equalizer bands
    for( int i = 0; i < EqualizerBands::kNbBands; i++)
    {
        stringstream ss;
        ss << "equalizer.band(" << i << ")";
        pVarManager->registerVar( m_varEqBands.getBand( i ), ss.str() );
    }

    // XXX WARNING XXX
    // The object variable callbacks are called from other VLC threads,
    // so they must put commands in the queue and NOT do anything else
    // (X11 calls are not reentrant)

#define ADD_CALLBACK( p_object, var ) \
    var_AddCallback( p_object, var, onGenericCallback, this );

    ADD_CALLBACK( pIntf->p_sys->p_playlist, "volume" )
    ADD_CALLBACK( pIntf->p_sys->p_playlist, "mute" )
    ADD_CALLBACK( pIntf->p_libvlc, "intf-toggle-fscontrol" )

    ADD_CALLBACK( pIntf->p_sys->p_playlist, "random" )
    ADD_CALLBACK( pIntf->p_sys->p_playlist, "loop" )
    ADD_CALLBACK( pIntf->p_sys->p_playlist, "repeat" )

#undef ADD_CALLBACK

    // Called when a playlist item is added
    var_AddCallback( pIntf->p_sys->p_playlist, "playlist-item-append",
                     onItemAppend, this );
    // Called when a playlist item is deleted
    // TODO: properly handle item-deleted
    var_AddCallback( pIntf->p_sys->p_playlist, "playlist-item-deleted",
                     onItemDelete, this );
    // Called when the current input changes
    var_AddCallback( pIntf->p_sys->p_playlist, "input-current",
                     onInputNew, this );
    // Called when a playlist item changed
    var_AddCallback( pIntf->p_sys->p_playlist, "item-change",
                     onItemChange, this );

    // Called when we have an interaction dialog to display
    var_Create( pIntf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( pIntf, "interaction", onInteraction, this );

    // initialize variables refering to liblvc and playlist objects
    init_variables();
}


VlcProc::~VlcProc()
{
    if( m_pVout )
    {
        vlc_object_release( m_pVout );
        m_pVout = NULL;
    }

    var_DelCallback( getIntf()->p_sys->p_playlist, "volume",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "mute",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_libvlc, "intf-toggle-fscontrol",
                     onGenericCallback, this );

    var_DelCallback( getIntf()->p_sys->p_playlist, "random",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "loop",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "repeat",
                     onGenericCallback, this );

    var_DelCallback( getIntf()->p_sys->p_playlist, "playlist-item-append",
                     onItemAppend, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "playlist-item-deleted",
                     onItemDelete, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "input-current",
                     onInputNew, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "item-change",
                     onItemChange, this );
    var_DelCallback( getIntf(), "interaction", onInteraction, this );
}

int VlcProc::onInputNew( vlc_object_t *pObj, const char *pVariable,
                         vlc_value_t oldval, vlc_value_t newval, void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldval;
    VlcProc *pThis = (VlcProc*)pParam;
    input_thread_t *pInput = static_cast<input_thread_t*>(newval.p_address);

    var_AddCallback( pInput, "intf-event", onGenericCallback2, pThis );
    var_AddCallback( pInput, "bit-rate", onGenericCallback, pThis );
    var_AddCallback( pInput, "sample-rate", onGenericCallback, pThis );
    var_AddCallback( pInput, "can-record", onGenericCallback, pThis );

    return VLC_SUCCESS;
}


int VlcProc::onItemChange( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldval;
    VlcProc *pThis = (VlcProc*)pParam;
    input_item_t *p_item = static_cast<input_item_t*>(newval.p_address);

    // Create a playtree notify command
    CmdItemUpdate *pCmdTree = new CmdItemUpdate( pThis->getIntf(),
                                                         p_item );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmdTree ), true );

    return VLC_SUCCESS;
}

int VlcProc::onItemAppend( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;

    playlist_add_t *p_add = static_cast<playlist_add_t*>(newVal.p_address);
    CmdPlaytreeAppend *pCmdTree =
        new CmdPlaytreeAppend( pThis->getIntf(), p_add );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmdTree ), false );

    return VLC_SUCCESS;
}

int VlcProc::onItemDelete( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;

    int i_id = newVal.i_int;
    CmdPlaytreeDelete *pCmdTree =
        new CmdPlaytreeDelete( pThis->getIntf(), i_id);

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmdTree ), false );

    return VLC_SUCCESS;
}

int VlcProc::onInteraction( vlc_object_t *pObj, const char *pVariable,
                            vlc_value_t oldVal, vlc_value_t newVal,
                            void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;
    interaction_dialog_t *p_dialog = (interaction_dialog_t *)(newVal.p_address);

    CmdInteraction *pCmd = new CmdInteraction( pThis->getIntf(), p_dialog );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
    return VLC_SUCCESS;
}

int VlcProc::onEqBandsChange( vlc_object_t *pObj, const char *pVariable,
                              vlc_value_t oldVal, vlc_value_t newVal,
                              void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;

    // Post a set equalizer bands command
    CmdSetEqBands *pCmd = new CmdSetEqBands( pThis->getIntf(),
                                             pThis->m_varEqBands,
                                             newVal.psz_string );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}


int VlcProc::onEqPreampChange( vlc_object_t *pObj, const char *pVariable,
                               vlc_value_t oldVal, vlc_value_t newVal,
                               void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;
    EqualizerPreamp *pVarPreamp = (EqualizerPreamp*)(pThis->m_cVarEqPreamp.get());

    // Post a set preamp command
    CmdSetEqPreamp *pCmd = new CmdSetEqPreamp( pThis->getIntf(), *pVarPreamp,
                                              (newVal.f_float + 20.0) / 40.0 );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}


int VlcProc::onGenericCallback( vlc_object_t *pObj, const char *pVariable,
                                vlc_value_t oldVal, vlc_value_t newVal,
                                void *pParam )
{
    (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );

#define ADD_CALLBACK_ENTRY( var, func, remove ) \
    { \
    if( strcmp( pVariable, var ) == 0 ) \
    { \
        string label = var; \
        CmdGeneric *pCmd = new CmdCallback( pThis->getIntf(), pObj, newVal, \
                                            &VlcProc::func, label ); \
        if( pCmd ) \
            pQueue->push( CmdGenericPtr( pCmd ), remove ); \
        return VLC_SUCCESS; \
    } \
    }

    ADD_CALLBACK_ENTRY( "volume", on_volume_changed, true )
    ADD_CALLBACK_ENTRY( "mute", on_mute_changed, true )

    ADD_CALLBACK_ENTRY( "bit-rate", on_bit_rate_changed, false )
    ADD_CALLBACK_ENTRY( "sample-rate", on_sample_rate_changed, false )
    ADD_CALLBACK_ENTRY( "can-record", on_can_record_changed, false )

    ADD_CALLBACK_ENTRY( "random", on_random_changed, false )
    ADD_CALLBACK_ENTRY( "loop", on_loop_changed, false )
    ADD_CALLBACK_ENTRY( "repeat", on_repeat_changed, false )

    ADD_CALLBACK_ENTRY( "audio-filter", on_audio_filter_changed, false )

    ADD_CALLBACK_ENTRY( "intf-toggle-fscontrol", on_intf_show_changed, false )

    ADD_CALLBACK_ENTRY( "mouse-moved", on_mouse_moved_changed, false )

#undef ADD_CALLBACK_ENTRY

    msg_Err( pThis->getIntf(), "no callback entry for %s", pVariable );
    return VLC_EGENERIC;
}


int VlcProc::onGenericCallback2( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam )
{
    (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );

    /**
     * For intf-event, commands are labeled based on the value of newVal.
     *
     * For some values (e.g position), only keep the latest command
     * when there are multiple pending commands (remove=true).
     *
     * for others, don't discard commands (remove=false)
     **/
    if( strcmp( pVariable, "intf-event" ) == 0 )
    {
        stringstream label;
        bool b_remove;
        switch( newVal.i_int )
        {
            case INPUT_EVENT_STATE:
            case INPUT_EVENT_POSITION:
            case INPUT_EVENT_RATE:
            case INPUT_EVENT_ES:
            case INPUT_EVENT_CHAPTER:
            case INPUT_EVENT_RECORD:
                b_remove = true;
                break;
            case INPUT_EVENT_VOUT:
            case INPUT_EVENT_AOUT:
            case INPUT_EVENT_DEAD:
                b_remove = false;
                break;
            default:
                return VLC_SUCCESS;
        }
        label <<  pVariable << "_" << newVal.i_int;
        CmdGeneric *pCmd = new CmdCallback( pThis->getIntf(), pObj, newVal,
                                            &VlcProc::on_intf_event_changed,
                                            label.str() );
        if( pCmd )
            pQueue->push( CmdGenericPtr( pCmd ), b_remove );

        return VLC_SUCCESS;
    }

    msg_Err( pThis->getIntf(), "no callback entry for %s", pVariable );
    return VLC_EGENERIC;
}


void VlcProc::on_intf_event_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    input_thread_t* pInput = (input_thread_t*) p_obj;

    assert( getIntf()->p_sys->p_input == NULL || getIntf()->p_sys->p_input == pInput );

    if( !getIntf()->p_sys->p_input )
    {
        msg_Dbg( getIntf(), "new input %p detected", pInput );

        getIntf()->p_sys->p_input = pInput;
        vlc_object_hold( pInput );

        // update global variables pertaining to this input
        update_current_input();

        // ensure the playtree is also updated
        // (highlights the new item to be played back)
        getPlaytreeVar().onUpdateCurrent( true );
    }

    switch( newVal.i_int )
    {
        case INPUT_EVENT_STATE:
        {
            int state = var_GetInteger( pInput, "state" );
            SET_BOOL( m_cVarStopped, false );
            SET_BOOL( m_cVarPlaying, state != PAUSE_S );
            SET_BOOL( m_cVarPaused, state == PAUSE_S );
            break;
        }

        case INPUT_EVENT_POSITION:
        {
            float pos = var_GetFloat( pInput, "position" );
            SET_STREAMTIME( m_cVarTime, pos, false );
            SET_BOOL( m_cVarSeekable, pos != 0.0 );
            break;
        }

        case INPUT_EVENT_RATE:
        {
            float rate = var_GetFloat( pInput, "rate" );
            char* buffer;
            if( asprintf( &buffer, "%.3g", rate ) != -1 )
            {
                SET_TEXT( m_cVarSpeed, UString( getIntf(), buffer ) );
                free( buffer );
            }
            break;
        }

        case INPUT_EVENT_ES:
        {
            // Do we have audio
            vlc_value_t audio_es;
            var_Change( pInput, "audio-es", VLC_VAR_CHOICESCOUNT,
                            &audio_es, NULL );
            SET_BOOL( m_cVarHasAudio, audio_es.i_int > 0 );
            break;
        }

        case INPUT_EVENT_VOUT:
        {
            vout_thread_t* pVout = input_GetVout( pInput );
            SET_BOOL( m_cVarHasVout, pVout != NULL );
            if( !pVout || pVout == m_pVout )
            {
                // end of input or vout reuse (nothing to do)
                if( pVout )
                    vlc_object_release( pVout );
                break;
            }
            if( m_pVout )
            {
                // remove previous Vout callbacks
                var_DelCallback( m_pVout, "mouse-moved",
                                 onGenericCallback, this );
                vlc_object_release( m_pVout );
                m_pVout = NULL;
            }

            // add new Vout callbackx
            var_AddCallback( pVout, "mouse-moved",
                             onGenericCallback, this );
            m_pVout = pVout;
            break;
        }

        case INPUT_EVENT_CHAPTER:
        {
            vlc_value_t chapters_count;
            var_Change( pInput, "chapter", VLC_VAR_CHOICESCOUNT,
                        &chapters_count, NULL );
            SET_BOOL( m_cVarDvdActive, chapters_count.i_int > 0 );
            break;
        }

        case INPUT_EVENT_RECORD:
            SET_BOOL( m_cVarRecording, var_GetBool( pInput, "record" ) );
            break;

        case INPUT_EVENT_DEAD:
            msg_Dbg( getIntf(), "end of input detected for %p", pInput );

            var_DelCallback( pInput, "intf-event", onGenericCallback2, this );
            var_DelCallback( pInput, "bit-rate", onGenericCallback, this );
            var_DelCallback( pInput, "sample-rate", onGenericCallback, this );
            var_DelCallback( pInput, "can-record" , onGenericCallback, this );
            vlc_object_release( pInput );
            getIntf()->p_sys->p_input = NULL;
            reset_input();
            break;

        default:
            break;
    }
}

void VlcProc::on_bit_rate_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)newVal;
    input_thread_t* pInput = (input_thread_t*) p_obj;

    assert( getIntf()->p_sys->p_input == NULL || getIntf()->p_sys->p_input == pInput );

    int bitrate = var_GetInteger( pInput, "bit-rate" ) / 1000;
    SET_TEXT( m_cVarStreamBitRate, UString::fromInt( getIntf(), bitrate ) );
}

void VlcProc::on_sample_rate_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)newVal;
    input_thread_t* pInput = (input_thread_t*) p_obj;

    assert( getIntf()->p_sys->p_input == NULL || getIntf()->p_sys->p_input == pInput );

    int sampleRate = var_GetInteger( pInput, "sample-rate" ) / 1000;
    SET_TEXT( m_cVarStreamSampleRate, UString::fromInt(getIntf(),sampleRate) );
}

void VlcProc::on_can_record_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)newVal;
    input_thread_t* pInput = (input_thread_t*) p_obj;

    assert( getIntf()->p_sys->p_input == NULL || getIntf()->p_sys->p_input == pInput );

    SET_BOOL( m_cVarRecordable, var_GetBool(  pInput, "can-record" ) );
}

void VlcProc::on_random_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)newVal;
    playlist_t* pPlaylist = (playlist_t*) p_obj;

    SET_BOOL( m_cVarRandom, var_GetBool( pPlaylist, "random" ) );
}

void VlcProc::on_loop_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)newVal;
    playlist_t* pPlaylist = (playlist_t*) p_obj;

    SET_BOOL( m_cVarLoop, var_GetBool( pPlaylist, "loop" ) );
}

void VlcProc::on_repeat_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)newVal;
    playlist_t* pPlaylist = (playlist_t*) p_obj;

    SET_BOOL( m_cVarRepeat, var_GetBool( pPlaylist, "repeat" ) );
}

void VlcProc::on_volume_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)p_obj; (void)newVal;
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;

    SET_VOLUME( m_cVarVolume, var_GetFloat( pPlaylist, "volume" ), false );
}

void VlcProc::on_mute_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)p_obj;
    SET_BOOL( m_cVarMute, newVal.b_bool );
}

void VlcProc::on_audio_filter_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)p_obj;
    char *pFilters = newVal.psz_string;
    bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
    SET_BOOL( m_cVarEqualizer, b_equalizer );
}

void VlcProc::on_intf_show_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)p_obj; (void)newVal;
    bool b_fullscreen = getFullscreenVar().get();

    if( !b_fullscreen )
    {
        if( newVal.b_bool )
        {
            // Create a raise all command
            CmdRaiseAll *pCmd = new CmdRaiseAll( getIntf(),
                getIntf()->p_sys->p_theme->getWindowManager() );

            // Push the command in the asynchronous command queue
            AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
            pQueue->push( CmdGenericPtr( pCmd ) );
        }
    }
    else
    {
        VoutManager* pVoutManager =  VoutManager::instance( getIntf() );
        FscWindow *pWin = pVoutManager->getFscWindow();
        if( pWin )
        {
            bool b_visible = pWin->getVisibleVar().get();
            AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );

            if( !b_visible )
            {
               CmdShowWindow* pCmd = new CmdShowWindow( getIntf(),
                             getIntf()->p_sys->p_theme->getWindowManager(),
                             *pWin );
               pQueue->push( CmdGenericPtr( pCmd ) );
            }
            else
            {
               CmdHideWindow* pCmd = new CmdHideWindow( getIntf(),
                              getIntf()->p_sys->p_theme->getWindowManager(),
                              *pWin );
               pQueue->push( CmdGenericPtr( pCmd ) );
            }
        }
    }
}

void VlcProc::on_mouse_moved_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)p_obj; (void)newVal;
    FscWindow* pFscWindow = VoutManager::instance( getIntf() )->getFscWindow();
    if( pFscWindow )
        pFscWindow->onMouseMoved();
}

void VlcProc::reset_input()
{
    SET_BOOL( m_cVarSeekable, false );
    SET_BOOL( m_cVarRecordable, false );
    SET_BOOL( m_cVarRecording, false );
    SET_BOOL( m_cVarDvdActive, false );
    SET_BOOL( m_cVarHasAudio, false );
    SET_BOOL( m_cVarHasVout, false );
    SET_BOOL( m_cVarStopped, true );
    SET_BOOL( m_cVarPlaying, false );
    SET_BOOL( m_cVarPaused, false );

    SET_STREAMTIME( m_cVarTime, 0, false );
    SET_TEXT( m_cVarStreamName, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamURI, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamBitRate, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamSampleRate, UString( getIntf(), "") );

    getPlaytreeVar().onUpdateCurrent( false );
}

void VlcProc::init_variables()
{
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;

    SET_BOOL( m_cVarRandom, var_GetBool( pPlaylist, "random" ) );
    SET_BOOL( m_cVarLoop, var_GetBool( pPlaylist, "loop" ) );
    SET_BOOL( m_cVarRepeat, var_GetBool( pPlaylist, "repeat" ) );

    SET_VOLUME( m_cVarVolume, var_GetFloat( pPlaylist, "volume" ), false );
    SET_BOOL( m_cVarMute, var_GetBool( pPlaylist, "mute" ) );

    SET_BOOL( m_cVarStopped, true );

    init_equalizer();
}


void VlcProc::update_current_input()
{
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;
    input_thread_t* pInput = getIntf()->p_sys->p_input;
    if( !pInput )
        return;

    input_item_t *pItem = input_GetItem( pInput );
    if( pItem )
    {
        // Update short name (as defined by --input-title-format)
        char *psz_fmt = var_InheritString( getIntf(), "input-title-format" );
        char *psz_name = str_format_meta( pPlaylist, psz_fmt );
        SET_TEXT( m_cVarStreamName, UString( getIntf(), psz_name ) );
        free( psz_fmt );
        free( psz_name );

        // Update local path (if possible) or full uri
        char *psz_uri = input_item_GetURI( pItem );
        char *psz_path = make_path( psz_uri );
        char *psz_save = psz_path ? psz_path : psz_uri;
        SET_TEXT( m_cVarStreamURI, UString( getIntf(), psz_save ) );
        free( psz_path );
        free( psz_uri );

        // Update art uri
        char *psz_art = input_item_GetArtURL( pItem );
        SET_STRING( m_cVarStreamArt, string( psz_art ? psz_art : "" ) );
        free( psz_art );
    }
}

void VlcProc::init_equalizer()
{
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;
    audio_output_t* pAout = playlist_GetAout( pPlaylist );
    if( pAout )
    {
        if( !var_Type( pAout, "equalizer-bands" ) )
            var_Create( pAout, "equalizer-bands",
                        VLC_VAR_STRING | VLC_VAR_DOINHERIT);
        if( !var_Type( pAout, "equalizer-preamp" ) )
            var_Create( pAout, "equalizer-preamp",
                        VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);

        // New Aout (addCallbacks)
        var_AddCallback( pAout, "audio-filter",
                         onGenericCallback, this );
        var_AddCallback( pAout, "equalizer-bands",
                         onEqBandsChange, this );
        var_AddCallback( pAout, "equalizer-preamp",
                         onEqPreampChange, this );
    }

    // is equalizer enabled ?
    char *pFilters = pAout ?
                   var_GetNonEmptyString( pAout, "audio-filter" ) :
                   var_InheritString( getIntf(), "audio-filter" );
    bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
    free( pFilters );
    SET_BOOL( m_cVarEqualizer, b_equalizer );

    // retrieve initial bands
    char* bands = pAout ?
                  var_GetString( pAout, "equalizer-bands" ) :
                  var_InheritString( getIntf(), "equalizer-bands" );
    if( bands )
    {
        m_varEqBands.set( bands );
        free( bands );
    }

    // retrieve initial preamp
    float preamp = pAout ?
                   var_GetFloat( pAout, "equalizer-preamp" ) :
                   var_InheritFloat( getIntf(), "equalizer-preamp" );
    EqualizerPreamp *pVarPreamp = (EqualizerPreamp*)m_cVarEqPreamp.get();
    pVarPreamp->set( (preamp + 20.0) / 40.0 );

    if( pAout )
        vlc_object_release( pAout);
}

void VlcProc::setFullscreenVar( bool b_fullscreen )
{
    SET_BOOL( m_cVarFullscreen, b_fullscreen );
}

#undef  SET_BOOL
#undef  SET_STREAMTIME
#undef  SET_TEXT
#undef  SET_STRING
#undef  SET_VOLUME
