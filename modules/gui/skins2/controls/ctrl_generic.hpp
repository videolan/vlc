/*****************************************************************************
 * ctrl_generic.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ctrl_generic.hpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#ifndef CTRL_GENERIC_HPP
#define CTRL_GENERIC_HPP

#include "../src/skin_common.hpp"
#include "../utils/pointer.hpp"
#include "../utils/fsm.hpp"
#include "../utils/ustring.hpp"

class EvtGeneric;
class OSGraphics;
class GenericLayout;
class Position;
class GenericWindow;


/// Base class for controls
class CtrlGeneric: public SkinObject
{
    public:
        virtual ~CtrlGeneric();

        /// Handle an event on the control
        virtual void handleEvent( EvtGeneric &rEvent ) {}

        /// Check whether coordinates are inside the control
        virtual bool mouseOver( int x, int y ) const { return false; }

        /// Draw the control on the given graphics
        virtual void draw( OSGraphics &rImage, int xDest, int yDest ) {}

        /// Set the position and the associated layout of the control
        virtual void setLayout( GenericLayout *pLayout,
                                const Position &rPosition );

        /// Get the position of the control in the layout, if any
        virtual const Position *getPosition() const { return m_pPosition; }

        /// Get the text of the tooltip
        virtual UString getTooltipText() const
            { return UString( getIntf(), "" ); }

        /// Overload this method if you want to do something special when
        /// the layout is resized
        virtual void onResize() {}

        /// Get the help text
        virtual const UString &getHelpText() const { return m_help; }

        /// Return true if the control can gain the focus
        virtual bool isFocusable() const { return false; }

    protected:
        CtrlGeneric( intf_thread_t *pIntf, const UString &rHelp );

        /// Tell the layout when the image has changed
        virtual void notifyLayout() const;

        /// Ask the layout to capture the mouse
        virtual void captureMouse() const;

        /// Ask the layout to release the mouse
        virtual void releaseMouse() const;

        /// Notify the window the tooltip has changed
        virtual void notifyTooltipChange() const;

        /// Get the associated window, if any
        virtual GenericWindow *getWindow() const;

        /// Overload this method if you want to do something special when
        /// the Position object is set
        virtual void onPositionChange() {}

    private:
        /// Associated layout
        GenericLayout *m_pLayout;
        /// Position in the layout
        Position *m_pPosition;
        /// Help text
        UString m_help;
};

typedef CountedPtr<CtrlGeneric> CtrlGenericPtr;


#endif
