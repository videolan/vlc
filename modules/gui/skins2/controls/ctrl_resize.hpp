/*****************************************************************************
 * ctrl_resize.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CTRL_RESIZE_HPP
#define CTRL_RESIZE_HPP

#include "ctrl_flat.hpp"
#include "../commands/cmd_generic.hpp"
#include "../src/window_manager.hpp"
#include "../utils/fsm.hpp"

class WindowManager;


/// Control decorator for resizing windows
class CtrlResize: public CtrlFlat
{
public:
    CtrlResize( intf_thread_t *pIntf, WindowManager &rWindowManager,
                CtrlFlat &rCtrl, GenericLayout &rLayout,
                const UString &rHelp, VarBool *pVisible,
                WindowManager::Direction_t direction );
    virtual ~CtrlResize() { }

    /// Handle an event
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the decorated control
    virtual bool mouseOver( int x, int y ) const;

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Set the position and the associated layout of the decorated control
    virtual void setLayout( GenericLayout *pLayout,
                            const Position &rPosition );
    virtual void unsetLayout();

    /// Get the position of the decorated control in the layout, if any
    virtual const Position *getPosition() const;

    /// Method called when the control is resized
    virtual void onResize();

    /// Get the type of control (custom RTTI)
    virtual std::string getType() const { return m_rCtrl.getType(); }

private:
    FSM m_fsm;
    /// Window manager
    WindowManager &m_rWindowManager;
    /// Decorated CtrlFlat
    CtrlFlat &m_rCtrl;
    /// The layout resized by this control
    GenericLayout &m_rLayout;
    /// The last received event
    EvtGeneric *m_pEvt;
    /// Position of the click that started the resizing
    int m_xPos, m_yPos;
    /// Direction of the resizing
    WindowManager::Direction_t m_direction;

    /// Change the cursor, based on the given direction
    void changeCursor( WindowManager::Direction_t direction ) const;

    /// Callback objects
    DEFINE_CALLBACK( CtrlResize, OutStill )
    DEFINE_CALLBACK( CtrlResize, StillOut )
    DEFINE_CALLBACK( CtrlResize, StillStill )
    DEFINE_CALLBACK( CtrlResize, StillResize )
    DEFINE_CALLBACK( CtrlResize, ResizeStill )
    DEFINE_CALLBACK( CtrlResize, ResizeResize )

    // Size of the layout, before resizing
    int m_width, m_height;
};

#endif

