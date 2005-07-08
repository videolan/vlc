/*****************************************************************************
 * ctrl_button.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
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

#ifndef CTRL_BUTTON_HPP
#define CTRL_BUTTON_HPP

#include "ctrl_generic.hpp"
#include "../utils/fsm.hpp"

class GenericBitmap;
class OSGraphics;
class CmdGeneric;


/// Base class for button controls
class CtrlButton: public CtrlGeneric
{
    public:
        /// Create a button with 3 images
        CtrlButton( intf_thread_t *pIntf, const GenericBitmap &rBmpUp,
                    const GenericBitmap &rBmpOver,
                    const GenericBitmap &rBmpDown,
                    CmdGeneric &rCommand, const UString &rTooltip,
                    const UString &rHelp, VarBool *pVisible );

        virtual ~CtrlButton();

        /// Handle an event
        virtual void handleEvent( EvtGeneric &rEvent );

        /// Check whether coordinates are inside the control
        virtual bool mouseOver( int x, int y ) const;

        /// Draw the control on the given graphics
        virtual void draw( OSGraphics &rImage, int xDest, int yDest );

        /// Get the text of the tooltip
        virtual UString getTooltipText() const { return m_tooltip; }

        /// Get the type of control (custom RTTI)
        virtual string getType() const { return "button"; }

    private:
        /// Finite state machine of the control
        FSM m_fsm;
        /// Command triggered by the button
        CmdGeneric &m_rCommand;
        /// Tooltip text
        const UString m_tooltip;
        /// Callbacks objects
        Callback m_cmdUpOverDownOver;
        Callback m_cmdDownOverUpOver;
        Callback m_cmdDownOverDown;
        Callback m_cmdDownDownOver;
        Callback m_cmdUpOverUp;
        Callback m_cmdUpUpOver;
        Callback m_cmdDownUp;
        Callback m_cmdUpHidden;
        Callback m_cmdHiddenUp;
        /// Images of the button in the different states
        OSGraphics *m_pImgUp, *m_pImgOver, *m_pImgDown;
        /// Current image
        OSGraphics *m_pImg;

        /// Callback functions
        static void transUpOverDownOver( SkinObject *pCtrl );
        static void transDownOverUpOver( SkinObject *pCtrl );
        static void transDownOverDown( SkinObject *pCtrl );
        static void transDownDownOver( SkinObject *pCtrl );
        static void transUpOverUp( SkinObject *pCtrl );
        static void transUpUpOver( SkinObject *pCtrl );
        static void transDownUp( SkinObject *pCtrl );
        static void transUpHidden( SkinObject *pCtrl );
        static void transHiddenUp( SkinObject *pCtrl );
};


#endif
