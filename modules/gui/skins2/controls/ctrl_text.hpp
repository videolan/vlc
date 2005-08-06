/*****************************************************************************
 * ctrl_text.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
        /// Create a text control with the optional given color
        CtrlText( intf_thread_t *pIntf, VarText &rVariable,
                  const GenericFont &rFont, const UString &rHelp,
                  uint32_t color, VarBool *pVisible );
        virtual ~CtrlText();

        /// Handle an event
        virtual void handleEvent( EvtGeneric &rEvent );

        /// Check whether coordinates are inside the control
        virtual bool mouseOver( int x, int y ) const;

        /// Draw the control on the given graphics
        virtual void draw( OSGraphics &rImage, int xDest, int yDest );

        /// Set the text of the control, with an optional color
        /// This takes effect immediatly
        void setText( const UString &rText, uint32_t color = 0xFFFFFFFF );

        /// Get the type of control (custom RTTI)
        virtual string getType() const { return "text"; }

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
        /// Image of the text
        GenericBitmap *m_pImg;
        /// Image of the text, repeated twice and with some blank between;
        /// useful to display a 'circular' moving text...
        GenericBitmap *m_pImgDouble;
        /// Current image (should always be equal to m_pImg or m_pImgDouble)
        GenericBitmap *m_pCurrImg;
        /// Position of the left side of the moving text
        int m_xPos;
        /// Offset between the mouse pointer and the left side of the
        /// moving text
        int m_xOffset;
         /// Timer to move the text
        OSTimer *m_pTimer;

        /// Callback for the timer
        static void updateText( SkinObject *pCtrl );

        /// Method called when the observed variable is modified
        virtual void onUpdate( Subject<VarText> &rVariable );

        /// Display the text on the control
        void displayText( const UString &rText );

        /// Helper function to set the position in the correct interval
        void adjust( int &position );

        /// Update the behaviour of the text whenever the control size changes
        virtual void onChangePosition();
};


#endif
