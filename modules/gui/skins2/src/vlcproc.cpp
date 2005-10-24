/*****************************************************************************
 * vlcproc.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <vlc/aout.h>
#include <vlc/vout.h>

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
#include "../utils/var_bool.hpp"


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


VlcProc::VlcProc( intf_thread_t *pIntf ): SkinObject( pIntf ), m_pVout( NULL ),
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
    pVarManager->registerVar( m_cVarRepeat, "playtree.isRepeat" );
    REGISTER_VAR( m_cVarTime, StreamTime, "time" )
    REGISTER_VAR( m_cVarVolume, Volume, "volume" )
    REGISTER_VAR( m_cVarMute, VarBoolImpl, "vlc.isMute" )
    REGISTER_VAR( m_cVarPlaying, VarBoolImpl, "vlc.isPlaying" )
    REGISTER_VAR( m_cVarStopped, VarBoolImpl, "vlc.isStopped" )
    REGISTER_VAR( m_cVarPaused, VarBoolImpl, "vlc.isPaused" )
    REGISTER_VAR( m_cVarSeekable, VarBoolImpl, "vlc.isSeekable" )
#undef REGISTER_VAR
    m_cVarStreamName = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamName, "streamName" );
    m_cVarStreamURI = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamURI, "streamURI" );

    // XXX WARNING XXX
    // The object variable callbacks are called from other VLC threads,
    // so they must put commands in the queue and NOT do anything else
    // (X11 calls are not reentrant)

    // Called when the playlist changes
    var_AddCallback( pIntf->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    // Called when a playlist item is added
    // TODO: properly handle item-append
    var_AddCallback( pIntf->p_sys->p_playlist, "item-append",
                     onIntfChange, this );
    // Called when a playlist item is deleted
    // TODO: properly handle item-deleted
    var_AddCallback( pIntf->p_sys->p_playlist, "item-deleted",
                     onIntfChange, this );
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
                     onIntfChange, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "item-deleted",
                     onIntfChange, this );
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
    // Did the user requested to quit vlc ?
    if( getIntf()->b_die || getIntf()->p_vlc->b_die )
    {
        CmdQuit *pCmd = new CmdQuit( getIntf() );
        AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    // Get the VLC variables
    StreamTime *pTime = (StreamTime*)m_cVarTime.get();
    Volume *pVolume = (Volume*)m_cVarVolume.get();
    VarBoolImpl *pVarPlaying = (VarBoolImpl*)m_cVarPlaying.get();
    VarBoolImpl *pVarStopped = (VarBoolImpl*)m_cVarStopped.get();
    VarBoolImpl *pVarPaused = (VarBoolImpl*)m_cVarPaused.get();
    VarBoolImpl *pVarSeekable = (VarBoolImpl*)m_cVarSeekable.get();
    VarBoolImpl *pVarMute = (VarBoolImpl*)m_cVarMute.get();
    VarBoolImpl *pVarRandom = (VarBoolImpl*)m_cVarRandom.get();
    VarBoolImpl *pVarLoop = (VarBoolImpl*)m_cVarLoop.get();
    VarBoolImpl *pVarRepeat = (VarBoolImpl*)m_cVarRepeat.get();

    // Refresh sound volume
    audio_volume_t volume;
    aout_VolumeGet( getIntf(), &volume );
    pVolume->set( (double)volume * 2.0 / AOUT_VOLUME_MAX );
    // Set the mute variable
    pVarMute->set( volume == 0 );

    // Update the input
    if( getIntf()->p_sys->p_input == NULL )
    {
        getIntf()->p_sys->p_input = (input_thread_t *)vlc_object_find(
            getIntf(), VLC_OBJECT_INPUT, FIND_ANYWHERE );
    }
    else if( getIntf()->p_sys->p_input->b_dead )
    {
        vlc_object_release( getIntf()->p_sys->p_input );
        getIntf()->p_sys->p_input = NULL;
    }

    input_thread_t *pInput = getIntf()->p_sys->p_input;

    if( pInput && !pInput->b_die )
    {
        // Refresh time variables
        vlc_value_t pos;
        var_Get( pInput, "position", &pos );
        pTime->set( pos.f_float, false );

        // Get the status of the playlist
        playlist_status_t status =
            getIntf()->p_sys->p_playlist->status.i_status;

        pVarPlaying->set( status == PLAYLIST_RUNNING );
        pVarStopped->set( status == PLAYLIST_STOPPED );
        pVarPaused->set( status == PLAYLIST_PAUSED );

        pVarSeekable->set( pos.f_float != 0.0 );
    }
    else
    {
        pVarPlaying->set( false );
        pVarPaused->set( false );
        pVarStopped->set( true );
        pVarSeekable->set( false );
        pTime->set( 0, false );
    }

    // Refresh the random variable
    vlc_value_t val;
    var_Get( getIntf()->p_sys->p_playlist, "random", &val );
    pVarRandom->set( val.b_bool != 0 );

    // Refresh the loop variable
    var_Get( getIntf()->p_sys->p_playlist, "loop", &val );
    pVarLoop->set( val.b_bool != 0 );

    // Refresh the repeat variable
    var_Get( getIntf()->p_sys->p_playlist, "repeat", &val );
    pVarRepeat->set( val.b_bool != 0 );
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
    playlist_t *p_playlist = (playlist_t*)pObj;
    pThis->updateStreamName(p_playlist);

    // Create a playlist notify command
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );
    // Create a playtree notify command
    CmdPlaytreeChanged *pCmdTree = new CmdPlaytreeChanged( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->remove( "notify playlist" );
    pQueue->remove( "playtree changed" );
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
        pQueue->remove( "raise all windows" );
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
    pQueue->remove( "notify playlist" );
    pQueue->remove( "playtree update" );
    pQueue->push( CmdGenericPtr( pCmd ) );
    pQueue->push( CmdGenericPtr( pCmdTree ) );

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

    // Create a playlist notify command
    // TODO: selective update
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );
    // Create a playtree notify command
    CmdPlaytreeChanged *pCmdTree = new CmdPlaytreeChanged( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    pQueue->remove( "notify playlist" );
    pQueue->remove( "playtree changed" );
    pQueue->push( CmdGenericPtr( pCmd ) );
    pQueue->push( CmdGenericPtr( pCmdTree ) );

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
    pQueue->remove( "change skin" );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}


void VlcProc::updateStreamName( playlist_t *p_playlist )
{
    if( p_playlist->p_input )
    {
        VarText &rStreamName = getStreamNameVar();
        VarText &rStreamURI = getStreamURIVar();
        // XXX: we should not need to access p_input->psz_source directly, a
        // getter should be provided by VLC core
        string name = p_playlist->p_input->input.p_item->psz_name;
        // XXX: This should be done in VLC core, not here...
        // Remove path information if any
        OSFactory *pFactory = OSFactory::instance( getIntf() );
        string::size_type pos = name.rfind( pFactory->getDirSeparator() );
        if( pos != string::npos )
        {
            name = name.substr( pos + 1, name.size() - pos + 1 );
        }
        UString srcName( getIntf(), name.c_str() );
        UString srcURI( getIntf(),
                         p_playlist->p_input->input.p_item->psz_uri );

        // Create commands to update the stream variables
        CmdSetText *pCmd1 = new CmdSetText( getIntf(), rStreamName, srcName );
        CmdSetText *pCmd2 = new CmdSetText( getIntf(), rStreamURI, srcURI );
        // Push the commands in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
        pQueue->push( CmdGenericPtr( pCmd1 ) );
        pQueue->push( CmdGenericPtr( pCmd2 ) );
    }
}


void *VlcProc::getWindow( intf_thread_t *pIntf, vout_thread_t *pVout,
                          int *pXHint, int *pYHint,
                          unsigned int *pWidthHint,
                          unsigned int *pHeightHint )
{
    VlcProc *pThis = pIntf->p_sys->p_vlcProc;
    pThis->m_pVout = pVout;
    if( pThis->m_handleSet.empty() )
    {
        return NULL;
    }
    else
    {
        // Get the window handle
        void *pWindow = *pThis->m_handleSet.begin();
        // Post a resize vout command
        CmdResizeVout *pCmd = new CmdResizeVout( pThis->getIntf(), pWindow,
                                                 *pWidthHint, *pHeightHint );
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->remove( "resize vout" );
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
        case VOUT_SET_ZOOM:
        {
            double fArg = va_arg( args, double );

            if( pThis->m_pVout )
            {
                // Compute requested vout dimensions
                int width = (int)( pThis->m_pVout->i_window_width * fArg );
                int height = (int)( pThis->m_pVout->i_window_height * fArg );

                // Post a resize vout command
                CmdResizeVout *pCmd =
                    new CmdResizeVout( pThis->getIntf(), pWindow,
                                       width, height );
                AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
                pQueue->remove( "resize vout" );
                pQueue->push( CmdGenericPtr( pCmd ) );
            }
        }

        default:
            msg_Dbg( pIntf, "control query not supported" );
            break;
    }

    return VLC_SUCCESS;
}

