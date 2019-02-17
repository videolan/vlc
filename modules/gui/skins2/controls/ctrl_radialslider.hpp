/*****************************************************************************
 * ctrl_radialslider.hpp
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

#ifndef CTRL_RADIALSLIDER_HPP
#define CTRL_RADIALSLIDER_HPP

#include "ctrl_generic.hpp"
#include "../utils/fsm.hpp"
#include "../utils/observer.hpp"


class GenericBitmap;
class OSGraphics;
class VarPercent;


/// Radial slider
class CtrlRadialSlider: public CtrlGeneric, public Observer<VarPercent>
{
public:
    /// Create a radial slider with the given image, which must be
    /// composed of numImg subimages of the same size
    CtrlRadialSlider( intf_thread_t *pIntf, const GenericBitmap &rBmpSeq,
                      int numImg, VarPercent &rVariable, float minAngle,
                      float maxAngle, const UString &rHelp,
                      VarBool *pVisible );

    virtual ~CtrlRadialSlider();

    /// Handle an event
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the control
    virtual bool mouseOver( int x, int y ) const;

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Get the type of control (custom RTTI)
    virtual std::string getType() const { return "radial_slider"; }

private:
    /// Finite state machine of the control
    FSM m_fsm;
    /// Number of sub-images in the slider image
    int m_numImg;
    /// Variable associated to the slider
    VarPercent &m_rVariable;
    /// Min and max angles of the button
    float m_minAngle, m_maxAngle;
    /// Position of the cursor
    int m_position;
    /// Size of an image
    int m_width, m_height;
    /// The last received event
    EvtGeneric *m_pEvt;
    /// Sequence of images
    const OSGraphics * const m_pImgSeq;

    /// Callback objects
    DEFINE_CALLBACK( CtrlRadialSlider, UpDown )
    DEFINE_CALLBACK( CtrlRadialSlider, DownUp )
    DEFINE_CALLBACK( CtrlRadialSlider, Move )

    /// Method called when the observed variable is modified
    virtual void onUpdate( Subject<VarPercent> &rVariable, void* );

    /// Change the position of the cursor, with the given position of
    /// the mouse (relative to the layout). Is blocking is true, the
    /// the cursor cannot do more than a half turn
    void setCursor( int posX, int posY, bool blocking );
};


#endif
