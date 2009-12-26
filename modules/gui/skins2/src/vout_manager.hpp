/*****************************************************************************
 * vout_manager.hpp
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Erwan Tulou < brezhoneg1 at yahoo.fr r>
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

#ifndef VOUTMANAGER_HPP
#define VOUTMANAGER_HPP

#include <vector>

#include <vlc_vout.h>
#include <vlc_vout_window.h>
#include "../utils/position.hpp"
#include "../commands/cmd_generic.hpp"
#include "../controls/ctrl_video.hpp"
#include "../events/evt_key.hpp"

class VarBool;
class GenericWindow;

#include <stdio.h>

class SavedWnd
{
public:
    SavedWnd( vout_window_t* pWnd, VoutWindow* pVoutWindow = NULL,
               CtrlVideo* pCtrlVideo = NULL, int height = 0, int width = 0 )
            : pWnd( pWnd ), pVoutWindow( pVoutWindow ),
              pCtrlVideo( pCtrlVideo ), height( height ), width( width ) { }
    ~SavedWnd() { }

    vout_window_t* pWnd;
    VoutWindow *pVoutWindow;
    CtrlVideo *pCtrlVideo;
    int height;
    int width;
};

class VoutMainWindow: public GenericWindow
{
public:

    VoutMainWindow( intf_thread_t *pIntf, int left = 0, int top = 0 ) :
            GenericWindow( pIntf, left, top, false, false, NULL,
                           GenericWindow::FullscreenWindow )
    {
        resize( 10, 10 );
        move( -50, -50 );
    }
    virtual ~VoutMainWindow() { }

    virtual void processEvent( EvtKey &rEvtKey )
    {
        // Only do the action when the key is down
        if( rEvtKey.getKeyState() == EvtKey::kDown )
            var_SetInteger( getIntf()->p_libvlc, "key-pressed",
                             rEvtKey.getModKey() );
    }
};


/// Singleton object handling VLC internal state and playlist
class VoutManager: public SkinObject
{
public:
    /// Get the instance of VoutManager
    /// Returns NULL if the initialization of the object failed
    static VoutManager *instance( intf_thread_t *pIntf );

    /// Delete the instance of VoutManager
    static void destroy( intf_thread_t *pIntf );

    /// Accept Wnd
    void* acceptWnd( vout_window_t* pWnd );

    /// Release Wnd
    void releaseWnd( vout_window_t* pWnd );

    /// set size Wnd
    void setSizeWnd( vout_window_t* pWnd, int width, int height );

    /// set fullscreen Wnd
    void setFullscreenWnd( vout_window_t* pWnd, bool b_fullscreen );

    /// Callback to request a vout window
    static void *getWindow( intf_thread_t *pIntf, vout_window_t *pWnd );

    // Window provider (release)
    static void releaseWindow( intf_thread_t *pIntf, vout_window_t *pWnd  );

    /// Callback to change a vout window
    static int controlWindow( struct vout_window_t *pWnd,
                              int query, va_list args );

    // Register Video Controls (when building theme)
    void registerCtrlVideo( CtrlVideo* p_CtrlVideo );

    // Register Video Controls (when building theme)
    void registerFSC( TopWindow* p_Win );

    // save and restore vouts (when changing theme)
    void saveVoutConfig( );
    void restoreVoutConfig( bool b_success );

    // save and restore vouts (when swapping Layout)
    void discardVout( CtrlVideo* pCtrlVideo );
    void requestVout( CtrlVideo* pCtrlVideo );

    // get a useable video Control
    CtrlVideo* getBestCtrlVideo( );

    // get the VoutMainWindow
    VoutMainWindow* getVoutMainWindow() { return m_pVoutMainWindow; }

    // test if vout are running
    bool hasVout() { return ( m_SavedWndVec.size() != 0 ) ; }

protected:
    // Protected because it is a singleton
    VoutManager( intf_thread_t *pIntf );
    virtual ~VoutManager();

private:

    vector<CtrlVideo *> m_pCtrlVideoVec;
    vector<CtrlVideo *> m_pCtrlVideoVecBackup;
    vector<SavedWnd> m_SavedWndVec;

    VoutMainWindow* m_pVoutMainWindow;

    TopWindow* m_pFscWindow;
};


#endif
