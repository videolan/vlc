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
#include <vlc_vout_display.h>

#include "vout_manager.hpp"
#include "window_manager.hpp"
#include "vlcproc.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_resize.hpp"
#include "../commands/cmd_voutwindow.hpp"
#include "../commands/cmd_on_top.hpp"



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
    delete pIntf->p_sys->p_voutManager;
    pIntf->p_sys->p_voutManager = NULL;
}


VoutManager::VoutManager( intf_thread_t *pIntf ): SkinObject( pIntf ),
     m_pVoutMainWindow( NULL ), m_pFscWindow( NULL ), m_pCtrlVideoVec(),
     m_pCtrlVideoVecBackup(), m_SavedWndVec()
{
    m_pVoutMainWindow = new VoutMainWindow( getIntf() );

    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    int width = pOsFactory->getScreenWidth();
    int height = pOsFactory->getScreenHeight();

    m_pVoutMainWindow->move( 0, 0 );
    m_pVoutMainWindow->resize( width, height );
}


VoutManager::~VoutManager( )
{
    delete m_pVoutMainWindow;
}


void VoutManager::registerCtrlVideo( CtrlVideo* p_CtrlVideo )
{
    m_pCtrlVideoVec.push_back( p_CtrlVideo );
}


void VoutManager::registerFSC( TopWindow* p_Win )
{
    m_pFscWindow = p_Win;

    int x = p_Win->getLeft();
    int y = p_Win->getTop();
    p_Win->setParent( m_pVoutMainWindow, x , y, 0, 0 );
}


void VoutManager::saveVoutConfig( )
{
    // Save width/height to be consistent across themes
    // and detach Video Controls
    vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); it++ )
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
    vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); it++ )
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
    vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); it++ )
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
    vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); it++ )
    {
        if( (*it).pCtrlVideo == NULL )
        {
            pCtrlVideo->attachVoutWindow( (*it).pVoutWindow,
                                          (*it).width, (*it).height );
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


void* VoutManager::acceptWnd( vout_window_t* pWnd )
{
    int width = (int)pWnd->cfg->width;
    int height = (int)pWnd->cfg->height;

    // Creation of a dedicated Window per vout thread
    VoutWindow* pVoutWindow = new VoutWindow( getIntf(), pWnd, width, height,
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
    m_SavedWndVec.push_back( SavedWnd( pWnd, pVoutWindow, pCtrlVideo ) );

    msg_Dbg( pWnd, "New vout : handle = %p, Ctrl = %p, w x h = %dx%d",
                    handle, pCtrlVideo, width, height );

    return handle;
}


void VoutManager::releaseWnd( vout_window_t *pWnd )
{
    // remove vout thread from savedVec
    vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); it++ )
    {
        if( (*it).pWnd == pWnd )
        {
            msg_Dbg( getIntf(), "vout released vout=%p, VideoCtrl=%p",
                             pWnd, (*it).pCtrlVideo );

            // if a video control was being used, detach from it
            if( (*it).pCtrlVideo )
            {
                (*it).pCtrlVideo->detachVoutWindow( );
            }

            // remove resources
            delete (*it).pVoutWindow;
            m_SavedWndVec.erase( it );
            break;
        }
    }
}


void VoutManager::setSizeWnd( vout_window_t *pWnd, int width, int height )
{
   msg_Dbg( pWnd, "setSize (%dx%d) received from vout threadr",
                  width, height );

   vector<SavedWnd>::iterator it;
   for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); it++ )
   {
       if( (*it).pWnd == pWnd )
       {
           VoutWindow* pVoutWindow = (*it).pVoutWindow;

           pVoutWindow->setOriginalWidth( width );
           pVoutWindow->setOriginalHeight( height );

           CtrlVideo* pCtrlVideo = pVoutWindow->getCtrlVideo();
           if( pCtrlVideo )
           {
               pCtrlVideo->resizeControl( width, height );
           }
           break;
       }
   }
}

void VoutManager::setFullscreenWnd( vout_window_t *pWnd, bool b_fullscreen )
{
    msg_Dbg( pWnd, "setFullscreen (%d) received from vout thread",
                    b_fullscreen ? 1 : 0 );

    VlcProc::instance( getIntf() )->setFullscreenVar( b_fullscreen );

    if( b_fullscreen )
    {
        m_pVoutMainWindow->show();
    }
    else
    {
        m_pVoutMainWindow->hide();
    }
}

// Functions called by window provider
// ///////////////////////////////////

void *VoutManager::getWindow( intf_thread_t *pIntf, vout_window_t *pWnd )
{
    // Theme may have been destroyed
    if( !pIntf->p_sys->p_theme )
        return NULL;

    vlc_mutex_lock( &pIntf->p_sys->vout_lock );
    pIntf->p_sys->b_vout_ready = false;
    pIntf->p_sys->handle = NULL;

    CmdNewVoutWindow *pCmd =
        new CmdNewVoutWindow( pIntf, pWnd );

    AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
    pQueue->push( CmdGenericPtr( pCmd ), false );

    while( !pIntf->p_sys->b_vout_ready )
        vlc_cond_wait( &pIntf->p_sys->vout_wait, &pIntf->p_sys->vout_lock );

    void* handle = pIntf->p_sys->handle;
    vlc_mutex_unlock( &pIntf->p_sys->vout_lock );

    return handle;
}


void VoutManager::releaseWindow( intf_thread_t *pIntf, vout_window_t *pWnd )
{
    vlc_mutex_lock( &pIntf->p_sys->vout_lock );
    pIntf->p_sys->b_vout_ready = false;

    CmdReleaseVoutWindow *pCmd =
        new CmdReleaseVoutWindow( pIntf, pWnd );

    AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
    pQueue->push( CmdGenericPtr( pCmd ), false );

    while( !pIntf->p_sys->b_vout_ready )
        vlc_cond_wait( &pIntf->p_sys->vout_wait, &pIntf->p_sys->vout_lock );

    vlc_mutex_unlock( &pIntf->p_sys->vout_lock );
}


int VoutManager::controlWindow( struct vout_window_t *pWnd,
                            int query, va_list args )
{
    intf_thread_t *pIntf = (intf_thread_t *)pWnd->sys;
    VoutManager *pThis = pIntf->p_sys->p_voutManager;

    switch( query )
    {
        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned int i_width  = va_arg( args, unsigned int );
            unsigned int i_height = va_arg( args, unsigned int );

            if( i_width && i_height )
            {
                // Post a vout resize command
                CmdResizeVout *pCmd =
                    new CmdResizeVout( pThis->getIntf(),
                                        pWnd, (int)i_width, (int)i_height );
                AsyncQueue *pQueue =
                   AsyncQueue::instance( pThis->getIntf() );
                pQueue->push( CmdGenericPtr( pCmd ) );
            }
            return VLC_EGENERIC;
        }

        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            bool b_fullscreen = va_arg( args, int );

            // Post a vout resize command
            CmdSetFullscreen* pCmd =
                new CmdSetFullscreen( pThis->getIntf(), pWnd, b_fullscreen );

            AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
            pQueue->push( CmdGenericPtr( pCmd ) );

            return VLC_SUCCESS;
        }

        case VOUT_WINDOW_SET_STATE:
        {
            unsigned i_arg = va_arg( args, unsigned );
            unsigned on_top = i_arg & VOUT_WINDOW_STATE_ABOVE;

            // Post a SetOnTop command
            CmdSetOnTop* pCmd =
                new CmdSetOnTop( pThis->getIntf(), on_top );

            AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
            pQueue->push( CmdGenericPtr( pCmd ) );

            return VLC_SUCCESS;
        }


        default:
            msg_Dbg( pWnd, "control query not supported" );
            return VLC_EGENERIC;
    }
}

