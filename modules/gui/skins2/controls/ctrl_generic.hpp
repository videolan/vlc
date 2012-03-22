/*****************************************************************************
 * ctrl_generic.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CTRL_GENERIC_HPP
#define CTRL_GENERIC_HPP

#include "../src/skin_common.hpp"
#include "../utils/pointer.hpp"
#include "../utils/ustring.hpp"
#include "../utils/observer.hpp"
#include "../commands/cmd_generic.hpp"

class Box;
class EvtGeneric;
class OSGraphics;
class GenericLayout;
class Position;
class TopWindow;
class VarBool;


/// Base class for controls
class CtrlGeneric: public SkinObject, public Observer<VarBool>
{
public:
    virtual ~CtrlGeneric();

    /// Handle an event on the control
    virtual void handleEvent( EvtGeneric &rEvent ) { (void)rEvent; }

    /// Check whether coordinates are inside the control
    virtual bool mouseOver( int x, int y ) const
        { (void)x; (void)y; return false; }

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest,
                       int yDest, int w, int h ) = 0;

    /// Set the position and the associated layout of the control
    virtual void setLayout( GenericLayout *pLayout,
                            const Position &rPosition );
    virtual void unsetLayout();

    /// Get the position of the control in the layout, if any
    virtual const Position *getPosition() const { return m_pPosition; }

    /// Get the text of the tooltip
    virtual UString getTooltipText() const
        { return UString( getIntf(), "" ); }

    /**
     * Overload this method if you want to do something special when
     * the layout is resized
     */
    virtual void onResize() { }

    /// Get the help text
    virtual const UString &getHelpText() const { return m_help; }

    /// Return true if the control can gain the focus
    virtual bool isFocusable() const { return false; }

    /// Return true if the control can be scrollable
    virtual bool isScrollable() const { return false; }

    /// Return true if the control is visible
    virtual bool isVisible() const;

    /// Get the type of control (custom RTTI)
    virtual string getType() const { return ""; }

protected:
    // If pVisible is NULL, the control is always visible
    CtrlGeneric( intf_thread_t *pIntf, const UString &rHelp,
                 VarBool *pVisible = NULL );

    /**
     * Tell the layout when the image has changed, with the size of the
     * rectangle to repaint and its offset.
     * Use the default values to repaint the whole window
     */
    virtual void notifyLayout( int witdh = -1, int height = -1,
                               int xOffSet = 0, int yOffSet = 0 );

    /**
     * Same as notifyLayout(), but takes optional images as parameters.
     * The maximum size(s) of the images will be used for repainting.
     */
    void notifyLayoutMaxSize( const Box *pImg1 = NULL,
                              const Box *pImg2 = NULL );

    /// Ask the layout to capture the mouse
    virtual void captureMouse() const;

    /// Ask the layout to release the mouse
    virtual void releaseMouse() const;

    /// Notify the window the tooltip has changed
    virtual void notifyTooltipChange() const;

    /// Get the associated window, if any
    virtual TopWindow *getWindow() const;

    /**
     * Overload this method if you want to do something special when
     * the Position object is set
     */
    virtual void onPositionChange() { }

    /// Overload this method to get notified of bool variable changes
    virtual void onVarBoolUpdate( VarBool &rVar ) { (void)rVar; }

    /// Method called when an observed bool variable is changed
    virtual void onUpdate( Subject<VarBool> &rVariable , void* );

    /// Associated layout
    GenericLayout *m_pLayout;

    /// Visibility variable
    VarBool *m_pVisible;

private:
    /// Position in the layout
    Position *m_pPosition;
    /// Help text
    UString m_help;

};

typedef CountedPtr<CtrlGeneric> CtrlGenericPtr;


#endif
