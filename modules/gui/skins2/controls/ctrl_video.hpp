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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CTRL_VIDEO_HPP
#define CTRL_VIDEO_HPP

#include "ctrl_generic.hpp"
#include "../utils/position.hpp"
#include "../src/vout_window.hpp"
#include <vlc_vout.h>


/// Control video
class CtrlVideo: public CtrlGeneric
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
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Get the type of control (custom RTTI)
    virtual string getType() const { return "video"; }

    /// Method called when visibility or ActiveLayout is updated
    virtual void onUpdate( Subject<VarBool> &rVariable , void* );

    // Attach a voutWindow to a Video Control
    void attachVoutWindow( VoutWindow* pVoutWindow,
                           int width = -1, int height = -1 );

    // Detach a voutWindow from a Video Control
    void detachVoutWindow( );

    // Get TopWindow associated with the video control
    virtual TopWindow* getWindow() const { return CtrlGeneric::getWindow(); }

    // Get the VoutWindow associated with the video control
    virtual VoutWindow* getVoutWindow() { return m_pVoutWindow; }

    /// Set the position and the associated layout of the control
    virtual void setLayout( GenericLayout *pLayout,
                            const Position &rPosition );
    virtual void unsetLayout();

    // resize the video Control
    virtual void resizeControl( int width, int height );

    // Is this control usable (visibility requirements)
    virtual bool isUseable() const;

    // Is this control used
    virtual bool isUsed() const;

private:
    /// Associated layout
    GenericLayout &m_rLayout;

    /// Autoresize parameter
    bool m_bAutoResize;

    /// Difference between layout size and video size
    int m_xShift, m_yShift;

    /// Vout window
    VoutWindow *m_pVoutWindow;
};

#endif
