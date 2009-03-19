/*****************************************************************************
 * vout_manager.cpp
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Erwan Tulou <brezhoneg1 at yahoo.fr>
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

#include <vlc_vout.h>
#include <vlc_window.h>

#include "vout_manager.hpp"
#include "window_manager.hpp"
#include "vlcproc.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_resize.hpp"



VoutManager *VoutManager::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_voutManager == NULL )
    {
        pIntf->p_sys->p_voutManager = new VoutManager( pIntf );
    }

    return pIntf->p_sys->p_voutManager;
}


void VoutManager::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_voutManager )
    {
        delete pIntf->p_sys->p_voutManager;
        pIntf->p_sys->p_voutManager = NULL;
    }
}


VoutManager::VoutManager( intf_thread_t *pIntf ): SkinObject( pIntf ),
     m_pVoutMainWindow( NULL ), m_pCtrlVideoVec(),
     m_pCtrlVideoVecBackup(), m_SavedVoutVec()
{
    vlc_mutex_init( &vout_lock );

    m_pVoutMainWindow = new VoutMainWindow( getIntf() );
}


VoutManager::~VoutManager( )
{
    vlc_mutex_destroy( &vout_lock );

    delete m_pVoutMainWindow;
}


void VoutManager::registerCtrlVideo( CtrlVideo* p_CtrlVideo )
{
    m_pCtrlVideoVec.push_back( p_CtrlVideo );
}


void VoutManager::saveVoutConfig( )
{
    // Save width/height to be consistent across themes
    // and detach Video Controls
    vector<SavedVout>::iterator it;
    for( it = m_SavedVoutVec.begin(); it != m_SavedVoutVec.end(); it++ )
    {
        if( (*it).pCtrlVideo )
        {
            // detach vout thread from VideoControl
            (*it).pCtrlVideo->detachVoutWindow( );

            // memorize width/height before VideoControl is destroyed
            (*it).width = (*it).pCtrlVideo->getPosition()->getWidth();
            (*it).height = (*it).pCtrlVideo->getPosition()->getHeight();
            (*it).pCtrlVideo = NULL;
       }
    }

    // Create a backup copy and reset original for new theme
    m_pCtrlVideoVecBackup = m_pCtrlVideoVec;
    m_pCtrlVideoVec.clear();
}


void VoutManager::restoreVoutConfig( bool b_success )
{
    if( !b_success )
    {
        // loading new theme failed, restoring previous theme
        m_pCtrlVideoVec = m_pCtrlVideoVecBackup;
    }

    // reattach vout(s) to Video Controls
    vector<SavedVout>::iterator it;
    for( it = m_SavedVoutVec.begin(); it != m_SavedVoutVec.end(); it++ )
    {
        CtrlVideo* pCtrlVideo = getBestCtrlVideo();
        if( pCtrlVideo )
        {
            pCtrlVideo->attachVoutWindow( (*it).pVoutWindow );
           (*it).pCtrlVideo = pCtrlVideo;
        }
    }
}


void VoutManager::discardVout( CtrlVideo* pCtrlVideo )
{
    vector<SavedVout>::iterator it;
    for( it = m_SavedVoutVec.begin(); it != m_SavedVoutVec.end(); it++ )
    {
        if( (*it).pCtrlVideo == pCtrlVideo )
        {
            // detach vout thread from VideoControl
            (*it).pCtrlVideo->detachVoutWindow( );
            (*it).width = (*it).pCtrlVideo->getPosition()->getWidth();
            (*it).height = (*it).pCtrlVideo->getPosition()->getHeight();
            (*it).pCtrlVideo = NULL;
            break;
        }
    }
}


void VoutManager::requestVout( CtrlVideo* pCtrlVideo )
{
    vector<SavedVout>::iterator it;
    for( it = m_SavedVoutVec.begin(); it != m_SavedVoutVec.end(); it++ )
    {
        if( (*it).pCtrlVideo == NULL )
        {
            pCtrlVideo->attachVoutWindow( (*it).pVoutWindow );
            (*it).pCtrlVideo = pCtrlVideo;
            break;
        }
    }
}


CtrlVideo* VoutManager::getBestCtrlVideo( )
{
    // try to find an unused useable VideoControl

    vector<CtrlVideo*>::const_iterator it;
    for( it = m_pCtrlVideoVec.begin(); it != m_pCtrlVideoVec.end(); it++ )
    {
        if( (*it)->isUseable() && !(*it)->isUsed() )
        {
            return (*it);
        }
    }

    return NULL;
}


void* VoutManager::acceptVout( vout_thread_t* pVout, int width, int height )
{
    // Creation of a dedicated Window per vout thread
    VoutWindow* pVoutWindow = new VoutWindow( getIntf(), pVout, width, height,
                                         (GenericWindow*) m_pVoutMainWindow );

    void* handle = pVoutWindow->getOSHandle();

    // try to find a video Control within the theme
    CtrlVideo* pCtrlVideo = getBestCtrlVideo();
    if( pCtrlVideo )
    {
        // A Video Control is available
        // directly attach vout thread to it
        pCtrlVideo->attachVoutWindow( pVoutWindow );
    }

    // save vout characteristics
    m_SavedVoutVec.push_back( SavedVout( pVout, pVoutWindow, pCtrlVideo ) );

    msg_Dbg( getIntf(), "New incoming vout=0x%p, handle=0x%p, VideoCtrl=0x%p",
                        pVout, handle, pCtrlVideo );

    return handle;
}


// Functions called by window provider
// ///////////////////////////////////

void *VoutManager::getWindow( intf_thread_t *pIntf, vout_window_t *pWnd )
{
    // Theme may have been destroyed
    if( !pIntf->p_sys->p_theme )
        return NULL;

    VoutManager *pThis = pIntf->p_sys->p_voutManager;

    vout_thread_t* pVout = pWnd->vout;
    int width = (int)pWnd->width;
    int height = (int)pWnd->height;

    pThis->lockVout();

    void* handle = pThis->acceptVout( pVout, width, height );

    pThis->unlockVout();

    return handle;
}


void VoutManager::releaseWindow( intf_thread_t *pIntf, vout_window_t *pWnd )
{
    VoutManager *pThis = pIntf->p_sys->p_voutManager;

    // Theme may have been destroyed
    if( !pIntf->p_sys->p_theme )
        return;

    vout_thread_t* pVout = pWnd->vout;

    pThis->lockVout();

    // remove vout thread from savedVec
    vector<SavedVout>::iterator it;
    for( it = pThis->m_SavedVoutVec.begin(); it != pThis->m_SavedVoutVec.end(); it++ )
    {
        if( (*it).pVout == pVout )
        {
            msg_Dbg( pIntf, "vout released vout=0x%p, VideoCtrl=0x%p",
                             pVout, (*it).pCtrlVideo );

            // if a video control was being used, detach from it
            if( (*it).pCtrlVideo )
            {
                (*it).pCtrlVideo->detachVoutWindow( );
            }

            // remove resources
            delete (*it).pVoutWindow;
            pThis->m_SavedVoutVec.erase( it );
            break;
        }
    }

    pThis->unlockVout();
}


int VoutManager::controlWindow( struct vout_window_t *pWnd,
                            int query, va_list args )
{
    intf_thread_t *pIntf = (intf_thread_t *)pWnd->p_private;
    VoutManager *pThis = pIntf->p_sys->p_voutManager;

    switch( query )
    {
        case VOUT_SET_SIZE:
        {
            unsigned int i_width  = va_arg( args, unsigned int );
            unsigned int i_height = va_arg( args, unsigned int );

            if( i_width && i_height )
            {
                // Post a resize vout command
                CmdResizeVout *pCmd =
                    new CmdResizeVout( pThis->getIntf(), pWnd->handle.hwnd,
                                       i_width, i_height );
                AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
                pQueue->push( CmdGenericPtr( pCmd ) );
            }
        }

        default:
            msg_Dbg( pWnd, "control query not supported" );
            break;
    }

    return VLC_SUCCESS;
}

