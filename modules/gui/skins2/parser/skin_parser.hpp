/*****************************************************************************
 * skin_parser.hpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifndef SKIN_PARSER_HPP
#define SKIN_PARSER_HPP

#include "xmlparser.hpp"
#include "builder_data.hpp"
#include <set>


/// Parser for the skin DTD
class SkinParser: public XMLParser
{
public:

    enum {
        POS_UNDEF  = 0,
        POS_CENTER = 1,
        POS_LEFT   = 2,
        POS_RIGHT  = 4,
        POS_TOP    = 8,
        POS_BOTTOM = 16,
    };

    SkinParser( intf_thread_t *pIntf, const string &rFileName,
                const string &rPath, BuilderData *pData = NULL );
    virtual ~SkinParser();

    const BuilderData &getData() const { return *m_pData; }

    static int convertColor( const char *transcolor );

private:
    /// Path of the theme
    const string m_path;
    /// Container for mapping data from the XML
    BuilderData *m_pData;
    /// Indicate whether the class owns the data
    bool m_ownData;
    /// Current IDs
    string m_curBitmapId;
    string m_curWindowId;
    string m_curLayoutId;
    string m_curPopupId;
    string m_curListId;
    string m_curTreeId;
    /// Current position of menu items in the popups
    list<int> m_popupPosList;
    /// Current offset of the controls
    int m_xOffset, m_yOffset;
    list<int> m_xOffsetList, m_yOffsetList;
    /// Stack of panel ids
    list<string> m_panelStack;
    /// Layer of the current control in the layout
    int m_curLayer;
    /// Set of used id
    set<string> m_idSet;

    /// Callbacks
    virtual void handleBeginElement( const string &rName,
                                     AttrList_t &attr );
    virtual void handleEndElement( const string &rName );

    /// Helper functions
    //@{
    bool convertBoolean( const char *value ) const;
    /// Transform to int, and check that it is in the given range (if not,
    /// the closest range boundary will be used)
    int convertInRange( const char *value, int minValue, int maxValue,
                        const string &rAttribute ) const;
    //@}

    /// Generate a new id
    const string generateId() const;

    /// Check if the id is unique, and if not generate a new one
    const string uniqueId( const string &id );

    /// Management of relative positions
    void getRefDimensions( int &rWidth, int &rHeight, bool toScreen );
    int getDimension( string value, int refDimension );
    int getPosition( string value );
    void updateWindowPos( int width, int height );

    void convertPosition( string position,
                          string xOffset, string yOffset,
                          string xMargin, string yMargin,
                          int width, int height, int refWidth, int refHeight,
                          int* p_x, int* p_y );

    /// Helper for handleBeginElement: Provide default attribute if missing.
    static void DefaultAttr( AttrList_t &attr, const char *a, const char *b )
    {
        if( attr.find(a) == attr.end() ) attr[strdup(a)] = strdup(b);
    }
    /// Helper for handleBeginElement: Complain if a named attribute is missing.
    bool MissingAttr( AttrList_t &attr, const string &name, const char *a );

};

#endif
