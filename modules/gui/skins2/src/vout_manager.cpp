/*****************************************************************************
 * vout_manager.cpp
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
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

#include "vout_manager.hpp"
#include "vlcproc.hpp"
#include "os_factory.hpp"



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
     m_pCtrlVideoVec(), m_pCtrlVideoVecBackup(), m_SavedWndVec(),
     m_pVoutMainWindow( NULL ), m_pFscWindow( NULL )
{
    m_pVoutMainWindow = new VoutMainWindow( getIntf() );

    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    int width = pOsFactory->getScreenWidth();
    int height = pOsFactory->getScreenHeight();

    m_pVoutMainWindow->move( 0, 0 );
    m_pVoutMainWindow->resize( width, height );

    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    rFullscreen.addObserver( this );
}


VoutManager::~VoutManager( )
{
    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    rFullscreen.delObserver( this );

    delete m_pVoutMainWindow;
}


void VoutManager::registerCtrlVideo( CtrlVideo* p_CtrlVideo )
{
    m_pCtrlVideoVec.push_back( p_CtrlVideo );
}


void VoutManager::registerFSC( FscWindow* p_Win )
{
    m_pFscWindow = p_Win;
}


void VoutManager::saveVoutConfig( )
{
    // Save width/height to be consistent across themes
    // and detach Video Controls
    std::vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); ++it )
    {
        if( it->pCtrlVideo )
        {
            // detach vout thread from VideoControl
            it->pCtrlVideo->detachVoutWindow( );

            // memorize width/height before VideoControl is destroyed
            it->width = it->pCtrlVideo->getPosition()->getWidth();
            it->height = it->pCtrlVideo->getPosition()->getHeight();
            it->pCtrlVideo = NULL;
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
    std::vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); ++it )
    {
        CtrlVideo* pCtrlVideo = getBestCtrlVideo();
        if( pCtrlVideo )
        {
            pCtrlVideo->attachVoutWindow( it->pVoutWindow );
            it->pCtrlVideo = pCtrlVideo;
        }
    }
}


void VoutManager::discardVout( CtrlVideo* pCtrlVideo )
{
    std::vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); ++it )
    {
        if( it->pCtrlVideo == pCtrlVideo )
        {
            // detach vout thread from VideoControl
            it->pCtrlVideo->detachVoutWindow( );
            it->width = it->pCtrlVideo->getPosition()->getWidth();
            it->height = it->pCtrlVideo->getPosition()->getHeight();
            it->pCtrlVideo = NULL;
            break;
        }
    }
}


void VoutManager::requestVout( CtrlVideo* pCtrlVideo )
{
    std::vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); ++it )
    {
        if( it->pCtrlVideo == NULL )
        {
            pCtrlVideo->attachVoutWindow( it->pVoutWindow,
                                          it->width, it->height );
            it->pCtrlVideo = pCtrlVideo;
            break;
        }
    }
}


CtrlVideo* VoutManager::getBestCtrlVideo( )
{
    std::vector<CtrlVideo*>::const_iterator it;

    // first, look up a video control that is visible and unused
    for( it = m_pCtrlVideoVec.begin(); it != m_pCtrlVideoVec.end(); ++it )
    {
        if( (*it)->isUseable() && !(*it)->isUsed() )
        {
            return (*it);
        }
    }

    return NULL;
}



// Functions called by window provider
// ///////////////////////////////////

void VoutManager::acceptWnd( vout_window_t* pWnd, int width, int height )
{
    // Creation of a dedicated Window per vout thread
    VoutWindow* pVoutWindow = new VoutWindow( getIntf(), pWnd, width, height,
                                         (GenericWindow*) m_pVoutMainWindow );

    // try to find a video Control within the theme
    CtrlVideo* pCtrlVideo = getBestCtrlVideo();
    if( pCtrlVideo )
    {
        // A Video Control is available
        // directly attach vout thread to it
        pCtrlVideo->attachVoutWindow( pVoutWindow );
    }
    else
    {
        pVoutWindow->setCtrlVideo( NULL );
    }

    // save vout characteristics
    m_SavedWndVec.push_back( SavedWnd( pWnd, pVoutWindow, pCtrlVideo ) );

    msg_Dbg( pWnd, "New vout : Ctrl = %p, w x h = %ix%i",
                    (void *)pCtrlVideo, width, height );
}


void VoutManager::releaseWnd( vout_window_t* pWnd )
{
    // remove vout thread from savedVec
    std::vector<SavedWnd>::iterator it;
    for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); ++it )
    {
        if( it->pWnd == pWnd )
        {
            msg_Dbg( getIntf(), "vout released vout=%p, VideoCtrl=%p",
                             (void *)pWnd, it->pCtrlVideo );

            // if a video control was being used, detach from it
            if( it->pCtrlVideo )
            {
                it->pCtrlVideo->detachVoutWindow( );
            }

            // remove resources
            delete it->pVoutWindow;
            m_SavedWndVec.erase( it );
            break;
        }
    }

    // force fullscreen to false so that user regains control
    VlcProc::instance( getIntf() )->setFullscreenVar( false );
}


void VoutManager::setSizeWnd( vout_window_t *pWnd, int width, int height )
{
   msg_Dbg( pWnd, "setSize (%ix%i) received from vout thread",
                  width, height );

   std::vector<SavedWnd>::iterator it;
   for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); ++it )
   {
       if( it->pWnd == pWnd )
       {
           VoutWindow* pVoutWindow = it->pVoutWindow;

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
    msg_Dbg( pWnd, "setFullscreen (%i) received from vout thread",
                   b_fullscreen );

    // reconfigure the fullscreen window (multiple screens)
    if( b_fullscreen )
    {
        std::vector<SavedWnd>::iterator it;
        for( it = m_SavedWndVec.begin(); it != m_SavedWndVec.end(); ++it )
        {
            if( it->pWnd == pWnd )
            {
                VoutWindow* pVoutWindow = it->pVoutWindow;
                configureFullscreen( *pVoutWindow );
                break;
            }
        }
    }

    // set fullscreen
    VlcProc::instance( getIntf() )->setFullscreenVar( b_fullscreen );
}


void VoutManager::hideMouseWnd( vout_window_t *pWnd, bool hide )
{
    msg_Dbg( pWnd, "hide mouse (%i) received from vout thread", hide );
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    if( hide )
        pOsFactory->changeCursor( OSFactory::kNoCursor );
    else
        pOsFactory->changeCursor( OSFactory::kDefaultArrow );
}


void VoutManager::onUpdate( Subject<VarBool> &rVariable, void *arg )
{
    (void)arg;
    VarBool &rFullscreen = VlcProc::instance( getIntf() )->getFullscreenVar();
    if( &rVariable == &rFullscreen )
    {
        if( rFullscreen.get() )
            m_pVoutMainWindow->show();
        else
            m_pVoutMainWindow->hide();
    }
}


void VoutManager::configureFullscreen( VoutWindow& rWindow )
{
    int numScr = var_InheritInteger( getIntf(), "qt-fullscreen-screennumber" );

    int x, y, w, h;
    if( numScr >= 0 )
    {
        // select screen requested by user
        OSFactory *pOsFactory = OSFactory::instance( getIntf() );
        pOsFactory->getMonitorInfo( numScr, &x, &y, &w, &h );
    }
    else
    {
        // select screen where display is already occurring
        rWindow.getMonitorInfo( &x, &y, &w, &h );
    }

    // move and resize fullscreen
    m_pVoutMainWindow->move( x, y );
    m_pVoutMainWindow->resize( w, h );

    // ensure the fs controller is also moved
    if( m_pFscWindow )
    {
        m_pFscWindow->moveTo( x, y, w, h );
    }

    // place voutWindow within fullscreen
    rWindow.move( x, y );
    rWindow.resize( w, h );
}
