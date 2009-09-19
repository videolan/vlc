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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_playlist.h>

#include "vlcproc.hpp"
#include "os_factory.hpp"
#include "os_loop.hpp"
#include "os_timer.hpp"
#include "var_manager.hpp"
#include "vout_manager.hpp"
#include "theme.hpp"
#include "window_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_change_skin.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_resize.hpp"
#include "../commands/cmd_vars.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_update_item.hpp"
#include "../commands/cmd_audio.hpp"
#include "../commands/cmd_callbacks.hpp"
#include "../utils/var_bool.hpp"
#include <sstream>


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


VlcProc::VlcProc( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_varVoutSize( pIntf ), m_varEqBands( pIntf ),
    m_pVout( NULL ), m_pAout( NULL ), m_bEqualizer_started( false ),
    m_cmdManage( this )
{
    // Create a timer to poll the status of the vlc
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    m_pTimer = pOsFactory->createOSTimer( m_cmdManage );
    m_pTimer->start( 100, false );

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
    m_cVarStreamName = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamName, "streamName" );
    m_cVarStreamURI = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamURI, "streamURI" );
    m_cVarStreamBitRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamBitRate, "bitrate" );
    m_cVarStreamSampleRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamSampleRate, "samplerate" );

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

    // Called when volume sound changes
#define ADD_CALLBACK( p_object, var ) \
    var_AddCallback( p_object, var, onGenericCallback, this );

    ADD_CALLBACK( pIntf->p_libvlc, "volume-change" )

    ADD_CALLBACK( pIntf->p_sys->p_playlist, "item-current" )
    ADD_CALLBACK( pIntf->p_sys->p_playlist, "random" )
    ADD_CALLBACK( pIntf->p_sys->p_playlist, "loop" )
    ADD_CALLBACK( pIntf->p_sys->p_playlist, "repeat" )

#undef ADD_CALLBACK

    // Called when the playlist changes
    var_AddCallback( pIntf->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    // Called when a playlist item is added
    var_AddCallback( pIntf->p_sys->p_playlist, "playlist-item-append",
                     onItemAppend, this );
    // Called when a playlist item is deleted
    // TODO: properly handle item-deleted
    var_AddCallback( pIntf->p_sys->p_playlist, "playlist-item-deleted",
                     onItemDelete, this );
    // Called when the "interface shower" wants us to show the skin
    var_AddCallback( pIntf->p_libvlc, "intf-show",
                     onIntfShow, this );
    // Called when the current input changes
    var_AddCallback( pIntf->p_sys->p_playlist, "input-current",
                     onInputNew, this );
    // Called when a playlist item changed
    var_AddCallback( pIntf->p_sys->p_playlist, "item-change",
                     onItemChange, this );
    // Called when our skins2 demux wants us to load a new skin
    var_AddCallback( pIntf, "skin-to-load", onSkinToLoad, this );

    // Called when we have an interaction dialog to display
    var_Create( pIntf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( pIntf, "interaction", onInteraction, this );
    interaction_Register( pIntf );

    // initialize variables refering to liblvc and playlist objects
    init_variables();
}


VlcProc::~VlcProc()
{
    m_pTimer->stop();
    delete( m_pTimer );

    if( m_pAout )
    {
        vlc_object_release( m_pAout );
        m_pAout = NULL;
    }
    if( m_pVout )
    {
        vlc_object_release( m_pVout );
        m_pVout = NULL;
    }

    if( getIntf()->p_sys->p_input )
        reset_input();

    interaction_Unregister( getIntf() );

    var_DelCallback( getIntf()->p_libvlc, "volume-change",
                     onGenericCallback, this );

    var_DelCallback( getIntf()->p_sys->p_playlist, "item-current",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "random",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "loop",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "repeat",
                     onGenericCallback, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "playlist-item-append",
                     onItemAppend, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "playlist-item-deleted",
                     onItemDelete, this );
    var_DelCallback( getIntf()->p_libvlc, "intf-show",
                     onIntfShow, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "input-current",
                     onInputNew, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "item-change",
                     onItemChange, this );
    var_DelCallback( getIntf(), "skin-to-load", onSkinToLoad, this );
    var_DelCallback( getIntf(), "interaction", onInteraction, this );
}

void VlcProc::manage()
{
    // Did the user request to quit vlc ?
    if( !vlc_object_alive( getIntf() ) )
    {
        // Get the instance of OSFactory
        OSFactory *pOsFactory = OSFactory::instance( getIntf() );

        // Exit the main OS loop
        pOsFactory->getOSLoop()->exit();

        return;
    }
}

void VlcProc::CmdManage::execute()
{
    // Just forward to VlcProc
    m_pParent->manage();
}


int VlcProc::onIntfChange( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;

    // Update the stream variable
    pThis->updateStreamName();

    // Create a playtree notify command (for new style playtree)
    CmdPlaytreeChanged *pCmdTree = new CmdPlaytreeChanged( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmdTree ) );

    return VLC_SUCCESS;
}


int VlcProc::onIntfShow( vlc_object_t *pObj, const char *pVariable,
                         vlc_value_t oldVal, vlc_value_t newVal,
                         void *pParam )
{
    if (newVal.b_bool)
    {
        VlcProc *pThis = (VlcProc*)pParam;

        // Create a raise all command
        CmdRaiseAll *pCmd = new CmdRaiseAll( pThis->getIntf(),
            pThis->getIntf()->p_sys->p_theme->getWindowManager() );

        // Push the command in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    return VLC_SUCCESS;
}

int VlcProc::onInputNew( vlc_object_t *pObj, const char *pVariable,
                         vlc_value_t oldval, vlc_value_t newval, void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;
    input_thread_t *pInput = static_cast<input_thread_t*>(newval.p_address);

    var_AddCallback( pInput, "intf-event", onGenericCallback, pThis );
    var_AddCallback( pInput, "bit-rate", onGenericCallback, pThis );
    var_AddCallback( pInput, "sample-rate", onGenericCallback, pThis );
    var_AddCallback( pInput, "can-record", onGenericCallback, pThis );

    return VLC_SUCCESS;
}


int VlcProc::onItemChange( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;
    input_item_t *p_item = static_cast<input_item_t*>(newval.p_address);

    // Update the stream variable
    pThis->updateStreamName();

    // Create a playtree notify command
    CmdPlaytreeUpdate *pCmdTree = new CmdPlaytreeUpdate( pThis->getIntf(),
                                                         p_item->i_id );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmdTree ), true );

    return VLC_SUCCESS;
}

int VlcProc::onItemAppend( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;

    playlist_add_t *p_add = (playlist_add_t*)malloc( sizeof(
                                                playlist_add_t ) ) ;

    memcpy( p_add, newVal.p_address, sizeof( playlist_add_t ) ) ;

    CmdGenericPtr ptrTree;
    CmdPlaytreeAppend *pCmdTree = new CmdPlaytreeAppend( pThis->getIntf(),
                                                             p_add );
    ptrTree = CmdGenericPtr( pCmdTree );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( ptrTree , false );

    return VLC_SUCCESS;
}

int VlcProc::onItemDelete( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;

    int i_id = newVal.i_int;

    CmdGenericPtr ptrTree;
    CmdPlaytreeDelete *pCmdTree = new CmdPlaytreeDelete( pThis->getIntf(),
                                                         i_id);
    ptrTree = CmdGenericPtr( pCmdTree );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( ptrTree , false );

    return VLC_SUCCESS;
}


int VlcProc::onSkinToLoad( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;

    // Create a playlist notify command
    CmdChangeSkin *pCmd =
        new CmdChangeSkin( pThis->getIntf(), newVal.psz_string );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}

int VlcProc::onInteraction( vlc_object_t *pObj, const char *pVariable,
                            vlc_value_t oldVal, vlc_value_t newVal,
                            void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;
    interaction_dialog_t *p_dialog = (interaction_dialog_t *)(newVal.p_address);

    CmdInteraction *pCmd = new CmdInteraction( pThis->getIntf(), p_dialog );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
    return VLC_SUCCESS;
}


void VlcProc::updateStreamName()
{
    // Create a update item command
    CmdUpdateItem *pCmdItem = new CmdUpdateItem( getIntf(), getStreamNameVar(), getStreamURIVar() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
    pQueue->push( CmdGenericPtr( pCmdItem ) );
}

int VlcProc::onEqBandsChange( vlc_object_t *pObj, const char *pVariable,
                              vlc_value_t oldVal, vlc_value_t newVal,
                              void *pParam )
{
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
    VlcProc *pThis = (VlcProc*)pParam;
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );

    CmdGeneric *pCmd = NULL;

#define ADD_CALLBACK_ENTRY( var, label ) \
    { \
    if( strcmp( pVariable, var ) == 0 ) \
        pCmd = new Cmd_##label( pThis->getIntf(), pObj, newVal ); \
    }

    ADD_CALLBACK_ENTRY( "item-current", item_current_changed )
    ADD_CALLBACK_ENTRY( "volume-change", volume_changed )

    ADD_CALLBACK_ENTRY( "intf-event", intf_event_changed )
    ADD_CALLBACK_ENTRY( "bit-rate", bit_rate_changed )
    ADD_CALLBACK_ENTRY( "sample-rate", sample_rate_changed )
    ADD_CALLBACK_ENTRY( "can-record", can_record_changed )

    ADD_CALLBACK_ENTRY( "random", random_changed )
    ADD_CALLBACK_ENTRY( "loop", loop_changed )
    ADD_CALLBACK_ENTRY( "repeat", repeat_changed )

    ADD_CALLBACK_ENTRY( "audio-filter", audio_filter_changed )

#undef ADD_CALLBACK_ENTRY

    if( pCmd )
        pQueue->push( CmdGenericPtr( pCmd ), false );
    else
        msg_Err( pObj, "no Callback entry provided for %s", pVariable );

    return VLC_SUCCESS;
}

void VlcProc::on_item_current_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    playlist_t * pPlaylist = getIntf()->p_sys->p_playlist;
    input_thread_t* pInput = getIntf()->p_sys->p_input;
    input_item_t *p_item = static_cast<input_item_t*>(newVal.p_address);

    if( pInput )
        reset_input();

    pInput = playlist_CurrentInput( pPlaylist );
    if( pInput )
        getIntf()->p_sys->p_input = pInput;

    // Update the stream variable
    updateStreamName();

    // Create a playtree notify command
    AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
    CmdPlaytreeUpdate *pCmdTree =
            new CmdPlaytreeUpdate( getIntf(), p_item->i_id );
    pQueue->push( CmdGenericPtr( pCmdTree ) , true );
}

void VlcProc::on_intf_event_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    input_thread_t* pInput = (input_thread_t*) p_obj;

    StreamTime *pTime = (StreamTime*)m_cVarTime.get();
    VarBoolImpl *pVarSeekable = (VarBoolImpl*)m_cVarSeekable.get();
    VarBoolImpl *pVarRecordable = (VarBoolImpl*)m_cVarRecordable.get();
    VarBoolImpl *pVarRecording  = (VarBoolImpl*)m_cVarRecording.get();
    VarBoolImpl *pVarDvdActive = (VarBoolImpl*)m_cVarDvdActive.get();
    VarBoolImpl *pVarHasVout = (VarBoolImpl*)m_cVarHasVout.get();
    VarBoolImpl *pVarHasAudio = (VarBoolImpl*)m_cVarHasAudio.get();
    VarBoolImpl *pVarFullscreen = (VarBoolImpl*)m_cVarFullscreen.get();
    VarBoolImpl *pVarPlaying = (VarBoolImpl*)m_cVarPlaying.get();
    VarBoolImpl *pVarStopped = (VarBoolImpl*)m_cVarStopped.get();
    VarBoolImpl *pVarPaused = (VarBoolImpl*)m_cVarPaused.get();
    VarBoolImpl *pVarEqualizer = (VarBoolImpl*)m_cVarEqualizer.get();

    switch( newVal.i_int )
    {
        case INPUT_EVENT_STATE:
        {
            int state = var_GetInteger( pInput, "state" );
            pVarStopped->set( false );
            pVarPlaying->set( state != PAUSE_S );
            pVarPaused->set( state == PAUSE_S );
            break;
        }

        case INPUT_EVENT_POSITION:
        {
            float pos = var_GetFloat( pInput, "position" );
            pTime->set( pos, false );
            pVarSeekable->set( pos != 0.0 );
            break;
        }

        case INPUT_EVENT_ES:
        {
            // Do we have audio
            vlc_value_t audio_es;
            var_Change( pInput, "audio-es", VLC_VAR_CHOICESCOUNT,
                            &audio_es, NULL );
            pVarHasAudio->set( audio_es.i_int > 0 );
            break;
        }

        case INPUT_EVENT_VOUT:
        {
            vout_thread_t* pVout = input_GetVout( pInput );
            pVarHasVout->set( pVout != NULL );
            if( pVout )
            {
                pVarFullscreen->set( var_GetBool( pVout, "fullscreen" ) );
                vlc_object_release( pVout );
            }
            break;
        }

        case INPUT_EVENT_AOUT:
        {
            aout_instance_t* pAout = input_GetAout( pInput );

            // end of input or aout reuse (nothing to do)
            if( !pAout || pAout == m_pAout )
            {
                if( pAout )
                    vlc_object_release( pAout );
                break;
            }

            // remove previous Aout if any
            if( m_pAout )
            {
                var_DelCallback( m_pAout, "audio-filter",
                                 onGenericCallback, this );
                if( m_bEqualizer_started )
                {
                    var_DelCallback( m_pAout, "equalizer-bands",
                                     onEqBandsChange, this );
                    var_DelCallback( m_pAout, "equalizer-preamp",
                                     onEqPreampChange, this );
                }
                vlc_object_release( m_pAout );
                m_pAout = NULL;
                m_bEqualizer_started = false;
            }

            // New Aout (addCallbacks)
            var_AddCallback( pAout, "audio-filter", onGenericCallback, this );

            char *pFilters = var_GetNonEmptyString( pAout, "audio-filter" );
            bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
            free( pFilters );
            pVarEqualizer->set( b_equalizer );
            if( b_equalizer )
            {
                var_AddCallback( pAout, "equalizer-bands",
                              onEqBandsChange, this );
                var_AddCallback( pAout, "equalizer-preamp",
                              onEqPreampChange, this );
                m_bEqualizer_started = true;
            }
            m_pAout = pAout;
            break;
        }

        case INPUT_EVENT_CHAPTER:
        {
            vlc_value_t chapters_count;
            var_Change( pInput, "chapter", VLC_VAR_CHOICESCOUNT,
                        &chapters_count, NULL );
            pVarDvdActive->set( chapters_count.i_int > 0 );
            break;
        }

        case INPUT_EVENT_RECORD:
            pVarRecording->set( var_GetBool( pInput, "record" ) );
            break;

        case INPUT_EVENT_DEAD:
            reset_input();
            break;

        default:
            break;
    }
}

void VlcProc::on_bit_rate_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    input_thread_t* pInput = (input_thread_t*) p_obj;

    VarText *pBitrate = (VarText*)m_cVarStreamBitRate.get();

    int bitrate = var_GetInteger( pInput, "bit-rate" ) / 1000;
    pBitrate->set( UString::fromInt( getIntf(), bitrate ) );
}

void VlcProc::on_sample_rate_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    input_thread_t* pInput = (input_thread_t*) p_obj;

    VarText *pSampleRate = (VarText*)m_cVarStreamSampleRate.get();

    int sampleRate = var_GetInteger( pInput, "sample-rate" ) / 1000;
    pSampleRate->set( UString::fromInt( getIntf(), sampleRate ) );
}

void VlcProc::on_can_record_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    input_thread_t* pInput = (input_thread_t*) p_obj;

    VarBoolImpl *pVarRecordable = (VarBoolImpl*)m_cVarRecordable.get();
    pVarRecordable->set( var_GetBool(  pInput, "can-record" ) );
}

void VlcProc::on_random_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    playlist_t* pPlaylist = (playlist_t*) p_obj;

    // Refresh the random variable
    VarBoolImpl *pVarRandom = (VarBoolImpl*)m_cVarRandom.get();
    pVarRandom->set( var_GetBool( pPlaylist, "random" ) );
}

void VlcProc::on_loop_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    playlist_t* pPlaylist = (playlist_t*) p_obj;

    // Refresh the loop variable
    VarBoolImpl *pVarLoop = (VarBoolImpl*)m_cVarLoop.get();
    pVarLoop->set( var_GetBool( pPlaylist, "loop" ) );
}

void VlcProc::on_repeat_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    playlist_t* pPlaylist = (playlist_t*) p_obj;

    // Refresh the repeat variable
    VarBoolImpl *pVarRepeat = (VarBoolImpl*)m_cVarRepeat.get();
    pVarRepeat->set( var_GetBool( pPlaylist, "repeat" ) );
}

void VlcProc::on_volume_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    (void)p_obj; (void)newVal;
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;

    // Refresh sound volume
    audio_volume_t volume;
    aout_VolumeGet( pPlaylist, &volume );
    Volume *pVolume = (Volume*)m_cVarVolume.get();
    pVolume->set( (double)volume * 2.0 / AOUT_VOLUME_MAX, false );

    // Set the mute variable
    VarBoolImpl *pVarMute = (VarBoolImpl*)m_cVarMute.get();
    pVarMute->set( volume == 0 );
}

void VlcProc::on_audio_filter_changed( vlc_object_t* p_obj, vlc_value_t newVal )
{
    aout_instance_t* pAout = (aout_instance_t*) p_obj;
    VarBoolImpl *pVarEqualizer = (VarBoolImpl*)m_cVarEqualizer.get();

    char *pFilters = newVal.psz_string;

    // Refresh the equalizer variable
    bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
    pVarEqualizer->set( b_equalizer );
    if( b_equalizer && !m_bEqualizer_started )
    {
        var_AddCallback( pAout, "equalizer-bands", onEqBandsChange, this );
        var_AddCallback( pAout, "equalizer-preamp", onEqPreampChange, this );
        m_bEqualizer_started = true;
    }
}

void VlcProc::reset_input()
{
    input_thread_t* pInput = getIntf()->p_sys->p_input;

    if( pInput )
    {
        StreamTime *pTime = (StreamTime*)m_cVarTime.get();
        VarBoolImpl *pVarSeekable = (VarBoolImpl*)m_cVarSeekable.get();
        VarBoolImpl *pVarRecordable = (VarBoolImpl*)m_cVarRecordable.get();
        VarBoolImpl *pVarRecording  = (VarBoolImpl*)m_cVarRecording.get();
        VarBoolImpl *pVarDvdActive = (VarBoolImpl*)m_cVarDvdActive.get();
        VarBoolImpl *pVarHasVout = (VarBoolImpl*)m_cVarHasVout.get();
        VarBoolImpl *pVarHasAudio = (VarBoolImpl*)m_cVarHasAudio.get();
        VarBoolImpl *pVarFullscreen = (VarBoolImpl*)m_cVarFullscreen.get();
        VarBoolImpl *pVarPlaying = (VarBoolImpl*)m_cVarPlaying.get();
        VarBoolImpl *pVarStopped = (VarBoolImpl*)m_cVarStopped.get();
        VarBoolImpl *pVarPaused = (VarBoolImpl*)m_cVarPaused.get();
        VarBoolImpl *pVarEqualizer = (VarBoolImpl*)m_cVarEqualizer.get();

        VarText *pBitrate = (VarText*)m_cVarStreamBitRate.get();
        VarText *pSampleRate = (VarText*)m_cVarStreamSampleRate.get();

        pVarSeekable->set( false );
        pVarRecordable->set( false );
        pVarRecording->set( false );
        pVarDvdActive->set( false );
        pTime->set( 0, false );
        pVarFullscreen->set( false );
        pVarHasAudio->set( false );
        pVarHasVout->set( false );
        pVarStopped->set( true );
        pVarPlaying->set( false );
        pVarPaused->set( false );
        pBitrate->set( UString( getIntf(), "") );
        pSampleRate->set( UString( getIntf(), "") );

        var_DelCallback( pInput, "intf-event", onGenericCallback, this );
        var_DelCallback( pInput, "bit-rate", onGenericCallback, this );
        var_DelCallback( pInput, "sample-rate", onGenericCallback, this );
        var_DelCallback( pInput, "can-record" , onGenericCallback, this );
        vlc_object_release( pInput );
        getIntf()->p_sys->p_input = NULL;
    }
}

void VlcProc::init_variables()
{
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;

    // Refresh the random variable
    VarBoolImpl *pVarRandom = (VarBoolImpl*)m_cVarRandom.get();
    pVarRandom->set( var_GetBool( pPlaylist, "random" ) );

    // Refresh the loop variable
    VarBoolImpl *pVarLoop = (VarBoolImpl*)m_cVarLoop.get();
    pVarLoop->set( var_GetBool( pPlaylist, "loop" ) );

    // Refresh the repeat variable
    VarBoolImpl *pVarRepeat = (VarBoolImpl*)m_cVarRepeat.get();
    pVarRepeat->set( var_GetBool( pPlaylist, "repeat" ) );

    // Refresh sound volume
    audio_volume_t volume;
    aout_VolumeGet( pPlaylist, &volume );
    Volume *pVolume = (Volume*)m_cVarVolume.get();
    pVolume->set( (double)volume * 2.0 / AOUT_VOLUME_MAX, false );

    // Set the mute variable
    VarBoolImpl *pVarMute = (VarBoolImpl*)m_cVarMute.get();
    pVarMute->set( volume == 0 );

    // Set the equalizer variable
    update_equalizer();
}

void VlcProc::update_equalizer()
{
    VarBoolImpl *pVarEqualizer = (VarBoolImpl*)m_cVarEqualizer.get();

    char *pFilters;
    if( m_pAout )
        pFilters = var_GetNonEmptyString( m_pAout, "audio-filter" );
    else
        pFilters = config_GetPsz( getIntf(), "audio-filter" );

    bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
    free( pFilters );
    pVarEqualizer->set( b_equalizer );
}

