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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VOUTMANAGER_HPP
#define VOUTMANAGER_HPP

#include <vector>

#include <vlc_vout.h>
#include <vlc_window.h>
#include "../utils/position.hpp"
#include "../commands/cmd_generic.hpp"
#include "../controls/ctrl_video.hpp"

class VarBool;
class GenericWindow;

#include <stdio.h>

class SavedVout
{
public:
    SavedVout( vout_thread_t* pVout, VoutWindow* pVoutWindow = NULL,
               CtrlVideo* pCtrlVideo = NULL, int height = 0, int width = 0 ) :
       pVout( pVout ), pVoutWindow( pVoutWindow ), pCtrlVideo( pCtrlVideo ),
       height( height ), width( width ) {}

    ~SavedVout() {}

    vout_thread_t* pVout;
    VoutWindow *pVoutWindow;
    CtrlVideo *pCtrlVideo;
    int height;
    int width;
};

class VoutMainWindow: public GenericWindow
{
    public:

        VoutMainWindow( intf_thread_t *pIntf, int left = 0, int top = 0 ) :
                GenericWindow( pIntf, left, top, false, false, NULL )
        {
            resize( 10, 10 );
            move( -50, -50 );
        }
        virtual ~VoutMainWindow() {}

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

        /// Callback to request a vout window
        static void *getWindow( intf_thread_t *pIntf, vout_window_t *pWnd );

        /// Accept Vout
        void* acceptVout( vout_thread_t* pVout, int width, int height );

        // Window provider (release)
        static void releaseWindow( intf_thread_t *pIntf, vout_window_t *pWnd  );

        /// Callback to change a vout window
        static int controlWindow( struct vout_window_t *pWnd,
                                  int query, va_list args );

        // Register Video Controls (when building theme)
        void registerCtrlVideo( CtrlVideo* p_CtrlVideo );

        // save and restore vouts (when changing theme)
        void saveVoutConfig( );
        void restoreVoutConfig( bool b_success );

        // save and restore vouts (when swapping Layout)
        void discardVout( CtrlVideo* pCtrlVideo );
        void requestVout( CtrlVideo* pCtrlVideo );

        // get a VoutWindow
        void* getHandle( vout_thread_t* pVout, int width, int height );

        // get a useable video Control
        CtrlVideo* getBestCtrlVideo( );

        // get the VoutMainWindow
        VoutMainWindow* getVoutMainWindow() { return m_pVoutMainWindow; }

        // (un)lock functions to protect vout sets
        void lockVout( ) { vlc_mutex_lock( &vout_lock ); }
        void unlockVout( ) { vlc_mutex_unlock( &vout_lock ); }

    protected:
        // Protected because it is a singleton
        VoutManager( intf_thread_t *pIntf );
        virtual ~VoutManager();

    private:

        vector<CtrlVideo *> m_pCtrlVideoVec;
        vector<CtrlVideo *> m_pCtrlVideoVecBackup;
        vector<SavedVout> m_SavedVoutVec;

        VoutMainWindow* m_pVoutMainWindow;

        vlc_mutex_t vout_lock;
};


#endif
