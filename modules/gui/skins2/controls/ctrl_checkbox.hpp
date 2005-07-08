/*****************************************************************************
 * ctrl_checkbox.hpp
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

#ifndef CTRL_CHECKBOX_HPP
#define CTRL_CHECKBOX_HPP

#include "ctrl_generic.hpp"
#include "../utils/fsm.hpp"
#include "../utils/observer.hpp"

class GenericBitmap;
class OSGraphics;
class CmdGeneric;


/// Base class for checkbox controls
class CtrlCheckbox: public CtrlGeneric
{
    public:
        /// Create a checkbox with 6 images
        CtrlCheckbox( intf_thread_t *pIntf,
                      const GenericBitmap &rBmpUp1,
                      const GenericBitmap &rBmpOver1,
                      const GenericBitmap &rBmpDown1,
                      const GenericBitmap &rBmpUp2,
                      const GenericBitmap &rBmpOver2,
                      const GenericBitmap &rBmpDown2,
                      CmdGeneric &rCommand1, CmdGeneric &rCommand2,
                      const UString &rTooltip1, const UString &rTooltip2,
                      VarBool &rVariable, const UString &rHelp,
                      VarBool *pVisible);

        virtual ~CtrlCheckbox();

        /// Handle an event
        virtual void handleEvent( EvtGeneric &rEvent );

        /// Check whether coordinates are inside the control
        virtual bool mouseOver( int x, int y ) const;

        /// Draw the control on the given graphics
        virtual void draw( OSGraphics &rImage, int xDest, int yDest );

        /// Get the text of the tooltip XXX
        virtual UString getTooltipText() const { return *m_pTooltip; }

        /// Get the type of control (custom RTTI)
        virtual string getType() const { return "checkbox"; }

    private:
        /// Finite state machine of the control
        FSM m_fsm;
        /// Observed variable
        VarBool &m_rVariable;
        /// Commands for the 2 states
        CmdGeneric &m_rCommand1, &m_rCommand2;
        /// Current command
        CmdGeneric *m_pCommand;
        /// Tooltip texts for the 2 states
        const UString m_tooltip1, m_tooltip2;
        /// Current tooltip
        const UString *m_pTooltip;
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
        /// Images of the checkbox in the different states
        OSGraphics *m_pImgUp1, *m_pImgOver1, *m_pImgDown1;
        OSGraphics *m_pImgUp2, *m_pImgOver2, *m_pImgDown2;
        /// Current set of images (pointing to 1 or 2)
        /// In fact, we consider here that a checkbox acts like 2 buttons, in a
        /// symetric way; this is a small trick to avoid multiplicating the
        /// callbacks (and it could be extended easily to support 3 buttons or
        /// more...)
        OSGraphics *m_pImgUp, *m_pImgOver, *m_pImgDown;
        /// Current image
        OSGraphics *m_pImgCurrent;

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

        /// Method called when the observed variable is modified
        virtual void onVarBoolUpdate( VarBool &rVariable );

        /// Helper function to update the current state of images
        void changeButton();
};


#endif
