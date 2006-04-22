/*****************************************************************************
 * ctrl_video.hpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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

#ifndef CTRL_VIDEO_HPP
#define CTRL_VIDEO_HPP

#include "ctrl_generic.hpp"
#include "../utils/position.hpp"

class VoutWindow;

/// Control video
class CtrlVideo: public CtrlGeneric, public Observer<VarBox, void*>
{
    public:
        CtrlVideo( intf_thread_t *pIntf, GenericLayout &rLayout,
                   bool autoResize, const UString &rHelp, VarBool *pVisible );
        virtual ~CtrlVideo();

        /// Handle an event on the control
        virtual void handleEvent( EvtGeneric &rEvent );

        /// Check whether coordinates are inside the control
        virtual bool mouseOver( int x, int y ) const;

        /// Callback for layout resize
        virtual void onResize();

        /// Called when the Position is set
        virtual void onPositionChange();

        /// Draw the control on the given graphics
        virtual void draw( OSGraphics &rImage, int xDest, int yDest );

        /// Get the type of control (custom RTTI)
        virtual string getType() const { return "video"; }

        /// Method called when the vout size is updated
        virtual void onUpdate( Subject<VarBox,void*> &rVoutSize, void* );

        /// Called by the layout when the control is show/hidden
        void setVisible( bool visible );

    private:
        /// Vout window
        VoutWindow *m_pVout;
        /// Associated layout
        GenericLayout &m_rLayout;
        /// Difference between layout size and video size
        int m_xShift, m_yShift;
};

#endif
