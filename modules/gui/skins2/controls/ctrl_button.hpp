/*****************************************************************************
 * ctrl_button.hpp
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

#ifndef CTRL_BUTTON_HPP
#define CTRL_BUTTON_HPP

#include "ctrl_generic.hpp"
#include "../utils/fsm.hpp"
#include "../src/anim_bitmap.hpp"

class GenericBitmap;
class CmdGeneric;


/// Base class for button controls
class CtrlButton: public CtrlGeneric, public Observer<AnimBitmap>
{
public:
    /// Create a button with 3 images
    CtrlButton( intf_thread_t *pIntf, const GenericBitmap &rBmpUp,
                const GenericBitmap &rBmpOver, const GenericBitmap &rBmpDown,
                CmdGeneric &rCommand, const UString &rTooltip,
                const UString &rHelp, VarBool *pVisible );

    virtual ~CtrlButton();

    /// Set the position and the associated layout of the control
    virtual void setLayout( GenericLayout *pLayout,
                            const Position &rPosition );
    virtual void unsetLayout();

    /// Handle an event
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the control
    virtual bool mouseOver( int x, int y ) const;

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Get the text of the tooltip
    virtual UString getTooltipText() const { return m_tooltip; }

    /// Get the type of control (custom RTTI)
    virtual std::string getType() const { return "button"; }

private:
    /// Finite state machine of the control
    FSM m_fsm;
    /// Command triggered by the button
    CmdGeneric &m_rCommand;
    /// Tooltip text
    const UString m_tooltip;
    /// Images of the button in the different states
    AnimBitmap m_imgUp, m_imgOver, m_imgDown;
    /// Current image
    AnimBitmap *m_pImg;

    /// Callback objects
    DEFINE_CALLBACK( CtrlButton, UpOverDownOver )
    DEFINE_CALLBACK( CtrlButton, DownOverUpOver )
    DEFINE_CALLBACK( CtrlButton, DownOverDown )
    DEFINE_CALLBACK( CtrlButton, DownDownOver )
    DEFINE_CALLBACK( CtrlButton, UpOverUp )
    DEFINE_CALLBACK( CtrlButton, UpUpOver )
    DEFINE_CALLBACK( CtrlButton, DownUp )
    DEFINE_CALLBACK( CtrlButton, UpHidden )
    DEFINE_CALLBACK( CtrlButton, HiddenUp )

    /// Change the current image
    void setImage( AnimBitmap *pImg );

    /// Method called when an animated bitmap changes
    virtual void onUpdate( Subject<AnimBitmap> &rBitmap, void* );

    /// Method called when visibility or ActiveLayout is updated
    virtual void onUpdate( Subject<VarBool> &rVariable , void* );

};


#endif
