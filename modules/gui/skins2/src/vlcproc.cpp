/*****************************************************************************
 * vlcproc.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teuli�e <ipkiss@via.ecp.fr>
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

#include <vlc/aout.h>
#include <vlc/vout.h>
#include <aout_internal.h>

#include "vlcproc.hpp"
#include "os_factory.hpp"
#include "os_timer.hpp"
#include "var_manager.hpp"
#include "theme.hpp"
#include "window_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_change_skin.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_resize.hpp"
#include "../commands/cmd_vars.hpp"
#include "../commands/cmd_dialogs.hpp"
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
    if( pIntf->p_sys->p_vlcProc )
    {
        delete pIntf->p_sys->p_vlcProc;
        pIntf->p_sys->p_vlcProc = NULL;
    }
}


VlcProc::VlcProc( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_varVoutSize( pIntf ), m_varEqBands( pIntf ),
    m_pVout( NULL ), m_pAout( NULL ), m_cmdManage( this )
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

    /* Playlist variables */
    REGISTER_VAR( m_cPlaylist, Playlist, "playlist" )
    pVarManager->registerVar( getPlaylistVar().getPositionVarPtr(),
                              "playlist.slider" );
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

    // Called when the playlist changes
    var_AddCallback( pIntf->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    // Called when a playlist item is added
    var_AddCallback( pIntf->p_sys->p_playlist, "item-append",
                     onItemAppend, this );
    // Called when a playlist item is deleted
    // TODO: properly handle item-deleted
    var_AddCallback( pIntf->p_sys->p_playlist, "item-deleted",
                     onItemDelete, this );
    // Called when the "interface shower" wants us to show the skin
    var_AddCallback( pIntf->p_sys->p_playlist, "intf-show",
                     onIntfShow, this );
    // Called when the current played item changes
    var_AddCallback( pIntf->p_sys->p_playlist, "playlist-current",
                     onPlaylistChange, this );
    // Called when a playlist item changed
    var_AddCallback( pIntf->p_sys->p_playlist, "item-change",
                     onItemChange, this );
    // Called when our skins2 demux wants us to load a new skin
    var_AddCallback( pIntf, "skin-to-load", onSkinToLoad, this );

    // Called when we have an interaction dialog to display
    var_Create( pIntf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( pIntf, "interaction", onInteraction, this );
    pIntf->b_interaction = VLC_TRUE;

    // Callbacks for vout requests
    getIntf()->pf_request_window = &getWindow;
    getIntf()->pf_release_window = &releaseWindow;
    getIntf()->pf_control_window = &controlWindow;

    getIntf()->p_sys->p_input = NULL;
}


VlcProc::~VlcProc()
{
    m_pTimer->stop();
    delete( m_pTimer );
    if( getIntf()->p_sys->p_input )
    {
        vlc_object_release( getIntf()->p_sys->p_input );
    }

    // Callbacks for vout requests
    getIntf()->pf_request_window = NULL;
    getIntf()->pf_release_window = NULL;
    getIntf()->pf_control_window = NULL;

    var_DelCallback( getIntf()->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "item-append",
                     onItemAppend, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "item-deleted",
                     onItemDelete, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "intf-show",
                     onIntfShow, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "playlist-current",
                     onPlaylistChange, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "item-change",
                     onItemChange, this );
    var_DelCallback( getIntf(), "skin-to-load", onSkinToLoad, this );
}


void VlcProc::registerVoutWindow( void *pVoutWindow )
{
    m_handleSet.insert( pVoutWindow );
    // Reparent the vout window
    if( m_pVout )
    {
        if( vout_Control( m_pVout, VOUT_REPARENT ) != VLC_SUCCESS )
            vout_Control( m_pVout, VOUT_CLOSE );
    }
}


void VlcProc::unregisterVoutWindow( void *pVoutWindow )
{
    m_handleSet.erase( pVoutWindow );
}


void VlcProc::dropVout()
{
    if( m_pVout )
    {
        if( vout_Control( m_pVout, VOUT_REPARENT ) != VLC_SUCCESS )
            vout_Control( m_pVout, VOUT_CLOSE );
        m_pVout = NULL;
    }
}


void VlcProc::manage()
{
    // Did the user request to quit vlc ?
    if( getIntf()->b_die || getIntf()->p_vlc->b_die )
    {
        CmdQuit *pCmd = new CmdQuit( getIntf() );
        AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    refreshPlaylist();
    refreshAudio();
    refreshInput();
}
void VlcProc::CmdManage::execute()
{
    // Just forward to VlcProc
    m_pParent->manage();
}

void VlcProc::refreshAudio()
{
    char *pFilters = NULL;

    // Check if the audio output has changed
    aout_instance_t *pAout = (aout_instance_t *)vlc_object_find( getIntf(),
            VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if( pAout )
    {
        if( pAout != m_pAout )
        {
            // Register the equalizer callbacks
            if( !var_AddCallback( pAout, "equalizer-bands",
                                  onEqBandsChange, this ) &&
                !var_AddCallback( pAout, "equalizer-preamp",
                                  onEqPreampChange, this ) )
            {
                m_pAout = pAout;
                //char * psz_bands = var_GetString( p_aout, "equalizer-bands" );
            }
        }
        // Get the audio filters
        pFilters = var_GetString( pAout, "audio-filter" );
        vlc_object_release( pAout );
    }
    else
    {
        // Get the audio filters
        pFilters = config_GetPsz( getIntf(), "audio-filter" );
    }

    // Refresh sound volume
    audio_volume_t volume;
    aout_VolumeGet( getIntf(), &volume );
    Volume *pVolume = (Volume*)m_cVarVolume.get();
    pVolume->set( (double)volume * 2.0 / AOUT_VOLUME_MAX );

    // Set the mute variable
    VarBoolImpl *pVarMute = (VarBoolImpl*)m_cVarMute.get();
    pVarMute->set( volume == 0 );

    // Refresh the equalizer variable
    VarBoolImpl *pVarEqualizer = (VarBoolImpl*)m_cVarEqualizer.get();
    pVarEqualizer->set( pFilters && strstr( pFilters, "equalizer" ) );
}

void VlcProc::refreshPlaylist()
{
    // Refresh the random variable
    VarBoolImpl *pVarRandom = (VarBoolImpl*)m_cVarRandom.get();
    vlc_value_t val;
    var_Get( getIntf()->p_sys->p_playlist, "random", &val );
    pVarRandom->set( val.b_bool != 0 );

    // Refresh the loop variable
    VarBoolImpl *pVarLoop = (VarBoolImpl*)m_cVarLoop.get();
    var_Get( getIntf()->p_sys->p_playlist, "loop", &val );
    pVarLoop->set( val.b_bool != 0 );

    // Refresh the repeat variable
    VarBoolImpl *pVarRepeat = (VarBoolImpl*)m_cVarRepeat.get();
    var_Get( getIntf()->p_sys->p_playlist, "repeat", &val );
    pVarRepeat->set( val.b_bool != 0 );
}

void VlcProc::refreshInput()
{
    StreamTime *pTime = (StreamTime*)m_cVarTime.get();
    VarBoolImpl *pVarSeekable = (VarBoolImpl*)m_cVarSeekable.get();
    VarBoolImpl *pVarDvdActive = (VarBoolImpl*)m_cVarDvdActive.get();
    VarBoolImpl *pVarHasVout = (VarBoolImpl*)m_cVarHasVout.get();
    VarBoolImpl *pVarHasAudio = (VarBoolImpl*)m_cVarHasAudio.get();
    VarText *pBitrate = (VarText*)m_cVarStreamBitRate.get();
    VarText *pSampleRate = (VarText*)m_cVarStreamSampleRate.get();
    VarBoolImpl *pVarFullscreen = (VarBoolImpl*)m_cVarFullscreen.get();
    VarBoolImpl *pVarPlaying = (VarBoolImpl*)m_cVarPlaying.get();
    VarBoolImpl *pVarStopped = (VarBoolImpl*)m_cVarStopped.get();
    VarBoolImpl *pVarPaused = (VarBoolImpl*)m_cVarPaused.get();

    input_thread_t *pInput = getIntf()->p_sys->p_input;

    // Update the input
    if( getIntf()->p_sys->p_input == NULL )
    {
        getIntf()->p_sys->p_input = getIntf()->p_sys->p_playlist->p_input;
        if( getIntf()->p_sys->p_input )
            vlc_object_yield( getIntf()->p_sys->p_input );
    }
    else if( getIntf()->p_sys->p_input->b_dead )
    {
        vlc_object_release( getIntf()->p_sys->p_input );
        getIntf()->p_sys->p_input = NULL;
    }


    if( pInput && !pInput->b_die )
    {
        // Refresh time variables
        vlc_value_t pos;
        var_Get( pInput, "position", &pos );
        pTime->set( pos.f_float, false );
        pVarSeekable->set( pos.f_float != 0.0 );

        // Refresh DVD detection
        vlc_value_t chapters_count;
        var_Change( pInput, "chapter", VLC_VAR_CHOICESCOUNT,
                        &chapters_count, NULL );
        pVarDvdActive->set( chapters_count.i_int > 0 );

        // Get the input bitrate
        int bitrate = var_GetInteger( pInput, "bit-rate" ) / 1000;
        pBitrate->set( UString::fromInt( getIntf(), bitrate ) );

        // Get the audio sample rate
        int sampleRate = var_GetInteger( pInput, "sample-rate" ) / 1000;
        pSampleRate->set( UString::fromInt( getIntf(), sampleRate ) );

        // Do we have audio
        vlc_value_t audio_es;
        var_Change( pInput, "audio-es", VLC_VAR_CHOICESCOUNT,
                        &audio_es, NULL );
        pVarHasAudio->set( audio_es.i_int > 0 );

        // Refresh fullscreen status
        vout_thread_t *pVout = (vout_thread_t *)vlc_object_find( pInput,
                                     VLC_OBJECT_VOUT, FIND_CHILD );
        pVarHasVout->set( pVout != NULL );
        if( pVout )
        {
            pVarFullscreen->set( pVout->b_fullscreen );
            vlc_object_release( pVout );
        }

        // Refresh play/pause status
        int state = var_GetInteger( pInput, "state" );
        pVarStopped->set( false );
        pVarPlaying->set( state != PAUSE_S );
        pVarPaused->set( state == PAUSE_S );
    }
    else
    {
        pVarSeekable->set( false );
        pVarDvdActive->set( false );
        pTime->set( 0, false );
        pVarFullscreen->set( false );
        pVarHasAudio->set( false );
        pVarHasVout->set( false );
        pVarStopped->set( true );
        pVarPlaying->set( false );
        pVarPaused->set( false );
    }
}

int VlcProc::onIntfChange( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;

    // Update the stream variable
    playlist_t *p_playlist = (playlist_t*)pObj;
    pThis->updateStreamName(p_playlist);

    // Create a playlist notify command (for old style playlist)
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );
    // Create a playtree notify command (for new style playtree)
    CmdPlaytreeChanged *pCmdTree = new CmdPlaytreeChanged( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
    pQueue->push( CmdGenericPtr( pCmdTree ) );

    return VLC_SUCCESS;
}


int VlcProc::onIntfShow( vlc_object_t *pObj, const char *pVariable,
                         vlc_value_t oldVal, vlc_value_t newVal,
                         void *pParam )
{
    if (newVal.i_int)
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


int VlcProc::onItemChange( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;

    // Update the stream variable
    playlist_t *p_playlist = (playlist_t*)pObj;
    pThis->updateStreamName(p_playlist);

    // Create a playlist notify command
    // TODO: selective update
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );
    // Create a playtree notify command
    CmdPlaytreeUpdate *pCmdTree = new CmdPlaytreeUpdate( pThis->getIntf(),
                                                         newVal.i_int );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
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

    // Create a playlist notify command (for old style playlist)
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
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

    // Create a playlist notify command (for old style playlist)
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
    pQueue->push( ptrTree , false );

    return VLC_SUCCESS;
}




int VlcProc::onPlaylistChange( vlc_object_t *pObj, const char *pVariable,
                               vlc_value_t oldVal, vlc_value_t newVal,
                               void *pParam )
{
    VlcProc *pThis = (VlcProc*)pParam;

    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );

    // Update the stream variable
    playlist_t *p_playlist = (playlist_t*)pObj;
    pThis->updateStreamName(p_playlist);

    // Create a playlist notify command (old style playlist)
    // TODO: selective update
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );
    // Create two playtree notify commands: one for old item, one for new
    CmdPlaytreeUpdate *pCmdTree = new CmdPlaytreeUpdate( pThis->getIntf(),
                                                         oldVal.i_int );
    pQueue->push( CmdGenericPtr( pCmdTree ) , true );
    pCmdTree = new CmdPlaytreeUpdate( pThis->getIntf(), newVal.i_int );
    pQueue->push( CmdGenericPtr( pCmdTree ) , true );

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


void VlcProc::updateStreamName( playlist_t *p_playlist )
{
    if( p_playlist && p_playlist->p_input )
    {
        // Get playlist item information
        input_item_t *pItem = p_playlist->p_input->input.p_item;

        VarText &rStreamName = getStreamNameVar();
        VarText &rStreamURI = getStreamURIVar();
        // XXX: we should not need to access p_input->psz_source directly, a
        // getter should be provided by VLC core
        string name = pItem->psz_name;
        // XXX: This should be done in VLC core, not here...
        // Remove path information if any
        OSFactory *pFactory = OSFactory::instance( getIntf() );
        string::size_type pos = name.rfind( pFactory->getDirSeparator() );
        if( pos != string::npos )
        {
            name = name.substr( pos + 1, name.size() - pos + 1 );
        }
        UString srcName( getIntf(), name.c_str() );
        UString srcURI( getIntf(), pItem->psz_uri );

       // Create commands to update the stream variables
        CmdSetText *pCmd1 = new CmdSetText( getIntf(), rStreamName, srcName );
        CmdSetText *pCmd2 = new CmdSetText( getIntf(), rStreamURI, srcURI );
        // Push the commands in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
        pQueue->push( CmdGenericPtr( pCmd1 ), false );
        pQueue->push( CmdGenericPtr( pCmd2 ), false );
    }
}


void *VlcProc::getWindow( intf_thread_t *pIntf, vout_thread_t *pVout,
                          int *pXHint, int *pYHint,
                          unsigned int *pWidthHint,
                          unsigned int *pHeightHint )
{
    VlcProc *pThis = pIntf->p_sys->p_vlcProc;
    if( pThis->m_handleSet.empty() )
    {
        return NULL;
    }
    else
    {
        pThis->m_pVout = pVout;
        // Get the window handle
        void *pWindow = *pThis->m_handleSet.begin();
        // Post a resize vout command
        CmdResizeVout *pCmd = new CmdResizeVout( pThis->getIntf(), pWindow,
                                                 *pWidthHint, *pHeightHint );
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ) );
        return pWindow;
    }
}


void VlcProc::releaseWindow( intf_thread_t *pIntf, void *pWindow )
{
    VlcProc *pThis = pIntf->p_sys->p_vlcProc;
    pThis->m_pVout = NULL;
}


int VlcProc::controlWindow( intf_thread_t *pIntf, void *pWindow,
                            int query, va_list args )
{
    VlcProc *pThis = pIntf->p_sys->p_vlcProc;

    switch( query )
    {
        case VOUT_SET_SIZE:
        {
            if( pThis->m_pVout )
            {
                unsigned int i_width  = va_arg( args, unsigned int );
                unsigned int i_height = va_arg( args, unsigned int );
                if( !i_width ) i_width = pThis->m_pVout->i_window_width;
                if( !i_height ) i_height = pThis->m_pVout->i_window_height;

                // Post a resize vout command
                CmdResizeVout *pCmd =
                    new CmdResizeVout( pThis->getIntf(), pWindow,
                                       i_width, i_height );
                AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
                pQueue->push( CmdGenericPtr( pCmd ) );
            }
        }

        default:
            msg_Dbg( pIntf, "control query not supported" );
            break;
    }

    return VLC_SUCCESS;
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

