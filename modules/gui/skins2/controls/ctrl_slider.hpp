/*****************************************************************************
 * ctrl_slider.hpp
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

#ifndef CTRL_SLIDER_HPP
#define CTRL_SLIDER_HPP

#include "ctrl_generic.hpp"
#include "../utils/bezier.hpp"
#include "../utils/fsm.hpp"
#include "../utils/observer.hpp"


class GenericBitmap;
class OSGraphics;
class VarPercent;


/// Cursor of a slider
class CtrlSliderCursor: public CtrlGeneric, public Observer<VarPercent>
{
    public:
        /// Create a cursor with 3 images (which are NOT copied, be careful)
        /// If pVisible is NULL, the control is always visible
        CtrlSliderCursor( intf_thread_t *pIntf, const GenericBitmap &rBmpUp,
                          const GenericBitmap &rBmpOver,
                          const GenericBitmap &rBmpDown,
                          const Bezier &rCurve, VarPercent &rVariable,
                          VarBool *pVisible, const UString &rTooltip,
                          const UString &rHelp );

        virtual ~CtrlSliderCursor();

        /// Handle an event
        virtual void handleEvent( EvtGeneric &rEvent );

        /// Check whether coordinates are inside the control
        virtual bool mouseOver( int x, int y ) const;

        /// Draw the control on the given graphics
        virtual void draw( OSGraphics &rImage, int xDest, int yDest );

        /// Get the text of the tooltip
        virtual UString getTooltipText() const { return m_tooltip; }

        /// Get the type of control (custom RTTI)
        virtual string getType() const { return "slider_cursor"; }

    private:
        /// Finite state machine of the control
        FSM m_fsm;
        /// Variable associated to the cursor
        VarPercent &m_rVariable;
        /// Tooltip text
        const UString m_tooltip;
        /// Initial size of the control
        int m_width, m_height;
        /// Callback objects
        Callback m_cmdOverDown;
        Callback m_cmdDownOver;
        Callback m_cmdOverUp;
        Callback m_cmdUpOver;
        Callback m_cmdMove;
        Callback m_cmdScroll;
        /// Position of the cursor
        int m_xPosition, m_yPosition;
        /// Last saved position of the cursor (stored as a percentage)
        float m_lastPercentage;
        /// Offset between the mouse pointer and the center of the cursor
        int m_xOffset, m_yOffset;
        /// The last received event
        EvtGeneric *m_pEvt;
        /// Images of the cursor in the differents states
        OSGraphics *m_pImgUp, *m_pImgOver, *m_pImgDown;
        /// Current image
        OSGraphics *m_pImg;
        /// Bezier curve of the slider
        const Bezier &m_rCurve;

        /// Callback functions
        static void transOverDown( SkinObject *pCtrl );
        static void transDownOver( SkinObject *pCtrl );
        static void transOverUp( SkinObject *pCtrl );
        static void transUpOver( SkinObject *pCtrl );
        static void transMove( SkinObject *pCtrl );
        static void transScroll( SkinObject *pCtrl );

        /// Method called when the position variable is modified
        virtual void onUpdate( Subject<VarPercent> &rVariable );

        /// Methode to compute the resize factors
        void getResizeFactors( float &rFactorX, float &rFactorY ) const;
};


/// Slider background
class CtrlSliderBg: public CtrlGeneric
{
    public:
        CtrlSliderBg( intf_thread_t *pIntf, CtrlSliderCursor &rCursor,
                      const Bezier &rCurve, VarPercent &rVariable,
                      int thickness, VarBool *pVisible, const UString &rHelp );
        virtual ~CtrlSliderBg() {}

        /// Tell whether the mouse is over the control
        virtual bool mouseOver( int x, int y ) const;

        /// Handle an event
        virtual void handleEvent( EvtGeneric &rEvent );

        /// Get the type of control (custom RTTI)
        virtual string getType() const { return "slider_bg"; }

    private:
        /// Cursor of the slider
        CtrlSliderCursor &m_rCursor;
        /// Variable associated to the slider
        VarPercent &m_rVariable;
        /// Thickness of the curve
        int m_thickness;
        /// Bezier curve of the slider
        const Bezier &m_rCurve;
        /// Initial size of the control
        int m_width, m_height;

        /// Methode to compute the resize factors
        void getResizeFactors( float &rFactorX, float &rFactorY ) const;
};


#endif
