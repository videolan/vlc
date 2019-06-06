/*****************************************************************************
 * ctrl_text.hpp
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

#ifndef CTRL_TEXT_HPP
#define CTRL_TEXT_HPP

#include "ctrl_generic.hpp"
#include "../utils/fsm.hpp"
#include "../utils/observer.hpp"
#include <string>

class GenericFont;
class GenericBitmap;
class OSTimer;
class UString;
class VarText;


/// Class for control text
class CtrlText: public CtrlGeneric, public Observer<VarText>
{
public:
    enum Align_t
    {
        kLeft,
        kCenter,
        kRight
    };

    enum Scrolling_t
    {
        // The text starts scrolling automatically if it is larger than the
        // width of the control. The user can still stop it or make it
        // scroll manually with the mouse.
        kAutomatic,
        // Only manual scrolling is allowed (with the mouse)
        kManual,
        // No scrolling of the text is allowed
        kNone
    };

    /// Create a text control with the optional given color
    CtrlText( intf_thread_t *pIntf, VarText &rVariable,
              const GenericFont &rFont, const UString &rHelp,
              uint32_t color, VarBool *pVisible, VarBool *pFocus,
              Scrolling_t scrollMode, Align_t alignment);
    virtual ~CtrlText();

    /// Handle an event
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the control
    virtual bool mouseOver( int x, int y ) const;

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Set the text of the control, with an optional color
    /// This takes effect immediatly
    void setText( const UString &rText, uint32_t color = 0xFFFFFFFF );

    /// Get the type of control (custom RTTI)
    virtual std::string getType() const { return "text"; }

private:
    /// Finite state machine of the control
    FSM m_fsm;
    /// Variable associated to the control
    VarText &m_rVariable;
    /// Callback objects
    DEFINE_CALLBACK( CtrlText, ToManual )
    DEFINE_CALLBACK( CtrlText, ManualMoving )
    DEFINE_CALLBACK( CtrlText, ManualStill )
    DEFINE_CALLBACK( CtrlText, Move )
    /// The last received event
    EvtGeneric *m_pEvt;
    /// Font used to render the text
    const GenericFont &m_rFont;
    /// Color of the text
    uint32_t m_color;
    /// Scrolling mode
    Scrolling_t m_scrollMode;
    /// Type of alignment
    Align_t m_alignment;
    /// indicate if control is focusable
    VarBool *m_pFocus;
    /// Image of the text
    GenericBitmap *m_pImg;
    /// Image of the text, repeated twice and with some blank between;
    /// useful to display a 'circular' moving text...
    GenericBitmap *m_pImgDouble;
    /// Current image (should always be equal to m_pImg or m_pImgDouble)
    GenericBitmap *m_pCurrImg;
    /// Position of the left side of the moving text (always <= 0)
    int m_xPos;
    /// Offset between the mouse pointer and the left side of the
    /// moving text
    int m_xOffset;
     /// Timer to move the text
    OSTimer *m_pTimer;

    /// Callback for the timer
    DEFINE_CALLBACK( CtrlText, UpdateText );

    /// Method called when the observed variable is modified
    virtual void onUpdate( Subject<VarText> &rVariable, void* );

    /// Method called when visibility is updated
    virtual void onUpdate( Subject<VarBool> &rVariable , void* );

    /// Intialize the set of pictures
    void setPictures( const UString &rText );

    /// Update object according to current context
    void updateContext();

    /// Helper function to set the position in the correct interval
    void adjust( int &position );

    /// Update the behaviour of the text whenever the control size changes
    virtual void onPositionChange();
    /// Update the behaviour of the text whenever the control size changes
    virtual void onResize();
};


#endif
