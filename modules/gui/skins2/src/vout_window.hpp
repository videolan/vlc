/*****************************************************************************
 * vout_window.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifndef VOUT_WINDOW_HPP
#define VOUT_WINDOW_HPP

#include "generic_window.hpp"

class OSGraphics;
class CtrlVideo;


/// Class to handle a video output window
class VoutWindow: private GenericWindow
{
    public:

        VoutWindow( intf_thread_t *pIntf, vout_thread_t* pVout,
                    int width, int height, GenericWindow* pParent = NULL );
        virtual ~VoutWindow();

        // counter used for debugging purpose
        static int count;

        /// Make some functions public
        //@{
        using GenericWindow::show;
        using GenericWindow::hide;
        using GenericWindow::move;
        using GenericWindow::getOSHandle;
        //@}

        /// Resize the window
        virtual void resize( int width, int height );

        /// get the parent  window
        virtual GenericWindow* getWindow( ) { return m_pParentWindow; }

        /// Refresh an area of the window
        virtual void refresh( int left, int top, int width, int height );

        /// set Video Control for VoutWindow
        virtual void setCtrlVideo( CtrlVideo* pCtrlVideo );

        /// get original size of vout
        virtual int getOriginalWidth( ) { return original_width; }
        virtual int getOriginalHeight( ) { return original_height; }

        virtual string getType() const { return "Vout"; }

    private:

        /// Image when there is no video
        OSGraphics *m_pImage;

        /// vout thread
        vout_thread_t* m_pVout;

        /// original width and height
        int original_width;
        int original_height;

        /// VideoControl attached to it
        CtrlVideo* m_pCtrlVideo;

        /// Parent Window
        GenericWindow* m_pParentWindow;
};

typedef CountedPtr<VoutWindow> VoutWindowPtr;

#endif
