/*****************************************************************************
 * vlcproc.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
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

#include "vlcproc.hpp"
#include "os_factory.hpp"
#include "os_timer.hpp"
#include "var_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_notify_playlist.hpp"
#include "../commands/cmd_quit.hpp"
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


VlcProc::VlcProc( intf_thread_t *pIntf ): SkinObject( pIntf )
{
    // Create a timer to poll the status of the vlc
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    m_pTimer = pOsFactory->createOSTimer( Callback( this, &doManage ) );
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
    REGISTER_VAR( m_cVarTime, Time, "time" )
    REGISTER_VAR( m_cVarVolume, Volume, "volume" )
    REGISTER_VAR( m_cVarStream, Stream, "stream" )
    REGISTER_VAR( m_cVarMute, VarBoolImpl, "vlc.isMute" ) // XXX broken
    REGISTER_VAR( m_cVarPlaying, VarBoolImpl, "vlc.isPlaying" )
    REGISTER_VAR( m_cVarStopped, VarBoolImpl, "vlc.isStopped" )
    REGISTER_VAR( m_cVarPaused, VarBoolImpl, "vlc.isPaused" )
    REGISTER_VAR( m_cVarSeekable, VarBoolImpl, "vlc.isSeekable" )
#undef REGISTER_VAR

    // Called when the playlist changes
    var_AddCallback( pIntf->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    // Called when the current played item changes
    var_AddCallback( pIntf->p_sys->p_playlist, "playlist-current",
                     onPlaylistChange, this );
    // Called when a playlist item changed
    var_AddCallback( pIntf->p_sys->p_playlist, "item-change",
                     onItemChange, this );

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
}


void VlcProc::manage()
{
    // Did the user requested to quit vlc ?
    if( getIntf()->p_vlc->b_die )
    {
        CmdQuit *pCmd = new CmdQuit( getIntf() );
        AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    // Get the VLC variables
    Time *pTime = (Time*)m_cVarTime.get();
    Volume *pVolume = (Volume*)m_cVarVolume.get();
    VarBoolImpl *pVarPlaying = (VarBoolImpl*)m_cVarPlaying.get();
    VarBoolImpl *pVarStopped = (VarBoolImpl*)m_cVarStopped.get();
    VarBoolImpl *pVarPaused = (VarBoolImpl*)m_cVarPaused.get();
    VarBoolImpl *pVarSeekable = (VarBoolImpl*)m_cVarSeekable.get();
    VarBoolImpl *pVarRandom = (VarBoolImpl*)m_cVarRandom.get();

    // Refresh sound volume
    audio_volume_t volume;
    aout_VolumeGet( getIntf(), &volume);
    pVolume->set( (double)volume / AOUT_VOLUME_MAX );

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
        if( pInput->stream.b_seekable )
        {
            // Refresh position in the stream
            vlc_value_t pos;
            var_Get( pInput, "position", &pos );
            if( pos.f_float >= 0.0 )
            {
                pTime->set( pos.f_float, false );
            }
        }
        else
        {
            pTime->set( 0, false );
        }

        // Get the status of the playlist
        playlist_status_t status = getIntf()->p_sys->p_playlist->i_status;

        pVarPlaying->set( status == PLAYLIST_RUNNING );
        pVarStopped->set( status == PLAYLIST_STOPPED );
        pVarPaused->set( status == PLAYLIST_PAUSED );
        pVarSeekable->set( pInput->stream.b_seekable );
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
    pVarRandom->set( val.b_bool );
}


void VlcProc::doManage( SkinObject *pObj )
{
    VlcProc *pThis = (VlcProc*)pObj;
    pThis->manage();
}


int VlcProc::onIntfChange( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = ( VlcProc* )pParam;

    // Create a playlist notify command
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->remove( "notify playlist" );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}


int VlcProc::onItemChange( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam )
{
    VlcProc *pThis = ( VlcProc* )pParam;

    // Create a playlist notify command
    // TODO: selective update
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->remove( "notify playlist" );
    pQueue->push( CmdGenericPtr( pCmd ) );
/*
    p_playlist_dialog->UpdateItem( new_val.i_int );*/
    return VLC_SUCCESS;
}


int VlcProc::onPlaylistChange( vlc_object_t *pObj, const char *pVariable,
                               vlc_value_t oldVal, vlc_value_t newVal,
                               void *pParam )
{
    VlcProc *pThis = ( VlcProc* )pParam;

    // Update the stream variable
    // XXX: we should not need to access p_inpu->psz_source directly, a
    // getter should be provided by VLC core
    playlist_t *p_playlist = (playlist_t*)pObj;
    if( p_playlist->p_input )
    {
        Stream *pStream = (Stream*)pThis->m_cVarStream.get();
        UString srcName( pThis->getIntf(),
                         p_playlist->p_input->psz_source );
        pStream->set( srcName, false );
    }

    // Create a playlist notify command
    // TODO: selective update
    CmdNotifyPlaylist *pCmd = new CmdNotifyPlaylist( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->remove( "notify playlist" );
    pQueue->push( CmdGenericPtr( pCmd ) );

//     p_playlist_dialog->UpdateItem( old_val.i_int );
//     p_playlist_dialog->UpdateItem( new_val.i_int );
    return VLC_SUCCESS;
}

