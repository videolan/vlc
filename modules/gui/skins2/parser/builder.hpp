/*****************************************************************************
 * builder.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: builder.hpp,v 1.4 2004/03/01 18:33:31 asmax Exp $
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

#ifndef BUILDER_HPP
#define BUILDER_HPP

#include "builder_data.hpp"
#include "../src/os_graphics.hpp"
#include "../src/generic_window.hpp"
#include "../src/generic_layout.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/generic_font.hpp"
#include "../commands/cmd_generic.hpp"
#include "../controls/ctrl_generic.hpp"
#include "../utils/bezier.hpp"

#include <string>
#include <list>
#include <map>

class Theme;


/// Class for skin construction
class Builder: public SkinObject
{
    public:
        Builder( intf_thread_t *pIntf, const BuilderData &rData );
        virtual ~Builder() {}

        /// Create a Theme object, ready to use.
        /// Return NULL in case of problem
        Theme *build();

        /// Parse an action tag and returns a command
        CmdGeneric *parseAction( const string &rAction );

    private:
        /// Data from the XML
        const BuilderData &m_rData;

        /// Theme under construction
        Theme *m_pTheme;

        void addTheme( const BuilderData::Theme &rData );
        void addBitmap( const BuilderData::Bitmap &rData );
        void addFont( const BuilderData::Font &rData );
        void addWindow( const BuilderData::Window &rData );
        void addLayout( const BuilderData::Layout &rData );
        void addAnchor( const BuilderData::Anchor &rData );
        void addButton( const BuilderData::Button &rData );
        void addCheckbox( const BuilderData::Checkbox &rData );
        void addImage( const BuilderData::Image &rData );
        void addText( const BuilderData::Text &rData );
        void addRadialSlider( const BuilderData::RadialSlider &rData );
        void addSlider( const BuilderData::Slider &rData );
        void addList( const BuilderData::List &rData );

       /// Compute the position of a control
        const Position makePosition( const string &rLeftTop,
                                     const string &rRightBottom,
                                     int xPos, int yPos, int width, int height,
                                     const Box &rBox ) const;

        /// Function to parse "points" tags
        Bezier *getPoints( const char *pTag ) const;
};

#endif

