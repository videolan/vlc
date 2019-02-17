/*****************************************************************************
 * ctrl_image.hpp
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

#ifndef CTRL_IMAGE_HPP
#define CTRL_IMAGE_HPP

#include "../commands/cmd_generic.hpp"
#include "../utils/observer.hpp"
#include "ctrl_flat.hpp"

class GenericBitmap;
class OSGraphics;
class CmdGeneric;
class VarString;


/// Control image
class CtrlImage: public CtrlFlat, public Observer<VarString>
{
public:
    /// Resize methods
    enum resize_t
    {
        kMosaic,                // Repeat the base image in a mosaic
        kScale,                 // Scale the base image
        kScaleAndRatioPreserved // Scale image (aspect ratio preserved)
    };

    // Create an image with the given bitmap (which is NOT copied)
    CtrlImage( intf_thread_t *pIntf, GenericBitmap &rBitmap,
               CmdGeneric &rCommand, resize_t resizeMethod,
               const UString &rHelp, VarBool *pVisible, bool art );
    virtual ~CtrlImage();

    /// Handle an event on the control
    virtual void handleEvent( EvtGeneric &rEvent );

    /// Check whether coordinates are inside the control
    virtual bool mouseOver( int x, int y ) const;

    /// Draw the control on the given graphics
    virtual void draw( OSGraphics &rImage, int xDest, int yDest, int w, int h );

    /// Get the type of control (custom RTTI)
    virtual std::string getType() const { return "image"; }

private:
    /// Bitmap
    GenericBitmap* m_pBitmap;
    /// original Bitmap
    GenericBitmap* m_pOriginalBitmap;
    /// Buffer to stored the rendered bitmap
    OSGraphics *m_pImage;
    /// Command triggered by a double-click on the image
    CmdGeneric &m_rCommand;
    /// Resize method
    resize_t m_resizeMethod;
    /// does the image get updated as art
    bool m_art;
    /// offset for image inside the control
    int m_x;
    int m_y;

    /// Method called when the observed variable is modified
    virtual void onUpdate( Subject<VarString> &rVariable, void* );


};

#endif
