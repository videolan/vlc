/*****************************************************************************
 * skin_parser.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "skin_parser.hpp"
#include "../src/os_factory.hpp"
#include "interpreter.hpp"
#include <math.h>

SkinParser::SkinParser( intf_thread_t *pIntf, const string &rFileName,
                        const string &rPath, bool useDTD, BuilderData *pData ):
    XMLParser( pIntf, rFileName, useDTD ), m_path( rPath), m_pData(pData),
    m_ownData(pData == NULL), m_xOffset( 0 ), m_yOffset( 0 )
{
    // Make sure the data is allocated
    if( m_pData == NULL )
    {
        m_pData = new BuilderData();
    }
}


SkinParser::~SkinParser()
{
    if( m_ownData )
    {
        delete m_pData;
    }
}


void SkinParser::handleBeginElement( const string &rName, AttrList_t &attr )
{
#define CheckDefault( a, b ) \
    if( attr.find(a) == attr.end() ) attr[strdup(a)] = strdup(b);
#define RequireDefault( a ) \
    if( attr.find(a) == attr.end() ) \
    { \
        msg_Err( getIntf(), "bad theme (element: %s, missing attribute: %s)", \
                 rName.c_str(), a ); \
        m_errors = true; return; \
    }

    if( rName == "Include" )
    {
        RequireDefault( "file" );

        OSFactory *pFactory = OSFactory::instance( getIntf() );
        string fullPath = m_path + pFactory->getDirSeparator() + attr["file"];
        msg_Dbg( getIntf(), "opening included XML file: %s", fullPath.c_str() );
        // FIXME: We do not use the DTD to validate the included XML file,
        // as the parser seems to dislike it otherwise...
        SkinParser subParser( getIntf(), fullPath.c_str(), false, m_pData );
        subParser.parse();
    }

    else if( rName == "IniFile" )
    {
        RequireDefault( "id" );
        RequireDefault( "file" );

        const BuilderData::IniFile iniFile( attr["id"],
                attr["file"] );
        m_pData->m_listIniFile.push_back( iniFile );
    }

    else if( rName == "Anchor" )
    {
        RequireDefault( "priority" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "points", "(0,0)" );
        CheckDefault( "range", "10" );

        const BuilderData::Anchor anchor( atoi( attr["x"] ) + m_xOffset,
                atoi( attr["y"] ) + m_yOffset, atoi( attr["range"] ),
                atoi( attr["priority"] ), attr["points"], m_curLayoutId );
        m_pData->m_listAnchor.push_back( anchor );
    }

    else if( rName == "Bitmap" )
    {
        RequireDefault( "id" );
        RequireDefault( "file" );
        RequireDefault( "alphacolor" );
        CheckDefault( "nbframes", "1" );
        CheckDefault( "fps", "4" );

        m_curBitmapId = uniqueId( attr["id"] );
        const BuilderData::Bitmap bitmap( m_curBitmapId,
                attr["file"], convertColor( attr["alphacolor"] ),
                atoi( attr["nbframes"] ), atoi( attr["fps"] ) );
        m_pData->m_listBitmap.push_back( bitmap );
    }

    else if( rName == "SubBitmap" )
    {
        RequireDefault( "id" );
        RequireDefault( "x" );
        RequireDefault( "y" );
        RequireDefault( "width" );
        RequireDefault( "height" );
        CheckDefault( "nbframes", "1" );
        CheckDefault( "fps", "4" );

        const BuilderData::SubBitmap bitmap( attr["id"],
                m_curBitmapId, atoi( attr["x"] ), atoi( attr["y"] ),
                atoi( attr["width"] ), atoi( attr["height"] ),
                atoi( attr["nbframes"] ), atoi( attr["fps"] ) );
        m_pData->m_listSubBitmap.push_back( bitmap );
    }

    else if( rName == "BitmapFont" )
    {
        RequireDefault( "id" );
        RequireDefault( "file" );
        CheckDefault( "type", "digits" );

        const BuilderData::BitmapFont font( attr["id"],
                attr["file"], attr["type"] );
        m_pData->m_listBitmapFont.push_back( font );
    }

    else if( rName == "PopupMenu" )
    {
        RequireDefault( "id" );

        m_popupPosList.push_back(0);
        m_curPopupId = uniqueId( attr["id"] );
        const BuilderData::PopupMenu popup( m_curPopupId );
        m_pData->m_listPopupMenu.push_back( popup );
    }

    else if( rName == "MenuItem" )
    {
        RequireDefault( "label" );
        CheckDefault( "action", "none" );

        const BuilderData::MenuItem item( attr["label"], attr["action"],
                                          m_popupPosList.back(),
                                          m_curPopupId );
        m_pData->m_listMenuItem.push_back( item );
        m_popupPosList.back()++;
    }

    else if( rName == "MenuSeparator" )
    {
        const BuilderData::MenuSeparator sep( m_popupPosList.back(),
                                              m_curPopupId );
        m_pData->m_listMenuSeparator.push_back( sep );
        m_popupPosList.back()++;
    }

    else if( rName == "Button" )
    {
        RequireDefault( "up" );
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "down", "none" );
        CheckDefault( "over", "none" );
        CheckDefault( "action", "none" );
        CheckDefault( "tooltiptext", "" );
        CheckDefault( "help", "" );

        const BuilderData::Button button( uniqueId( attr["id"] ),
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["lefttop"], attr["rightbottom"], attr["visible"],
                attr["up"], attr["down"], attr["over"], attr["action"],
                attr["tooltiptext"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_pData->m_listButton.push_back( button );
    }

    else if( rName == "Checkbox" )
    {
        RequireDefault( "up1" );
        RequireDefault( "up2" );
        RequireDefault( "state" );
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "down1", "none" );
        CheckDefault( "over1", "none" );
        CheckDefault( "down2", "none" );
        CheckDefault( "over2", "none" );
        CheckDefault( "action1", "none" );
        CheckDefault( "action2", "none" );
        CheckDefault( "tooltiptext1", "" );
        CheckDefault( "tooltiptext2", "" );
        CheckDefault( "help", "" );

        const BuilderData::Checkbox checkbox( uniqueId( attr["id"] ),
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["lefttop"], attr["rightbottom"], attr["visible"],
                attr["up1"], attr["down1"], attr["over1"],
                attr["up2"], attr["down2"], attr["over2"], attr["state"],
                attr["action1"], attr["action2"], attr["tooltiptext1"],
                attr["tooltiptext2"], attr["help"], m_curLayer, m_curWindowId,
                m_curLayoutId );
        m_curLayer++;
        m_pData->m_listCheckbox.push_back( checkbox );
    }

    else if( rName == "Font" )
    {
        RequireDefault( "id" );
        RequireDefault( "file" );
        CheckDefault( "size", "12" );

        const BuilderData::Font fontData( uniqueId( attr["id"] ),
                attr["file"], atoi( attr["size"] ) );
        m_pData->m_listFont.push_back( fontData );
    }

    else if( rName == "Group" )
    {
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );

        m_xOffset += atoi( attr["x"] );
        m_yOffset += atoi( attr["y"] );
        m_xOffsetList.push_back( atoi( attr["x"] ) );
        m_yOffsetList.push_back( atoi( attr["y"] ) );
    }

    else if( rName == "Image" )
    {
        RequireDefault( "image" );
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "action", "none" );
        CheckDefault( "action2", "none" );
        CheckDefault( "resize", "mosaic" );
        CheckDefault( "help", "" );

        const BuilderData::Image imageData( uniqueId( attr["id"] ),
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["lefttop"], attr["rightbottom"], attr["visible"],
                attr["image"], attr["action"], attr["action2"], attr["resize"],
                attr["help"], m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_pData->m_listImage.push_back( imageData );
    }

    else if( rName == "Layout" )
    {
        RequireDefault( "width" );
        RequireDefault( "height" );
        CheckDefault( "id", "none" );
        CheckDefault( "minwidth", "-1" );
        CheckDefault( "maxwidth", "-1" );
        CheckDefault( "minheight", "-1" );
        CheckDefault( "maxheight", "-1" );

        m_curLayoutId = uniqueId( attr["id"] );
        const BuilderData::Layout layout( m_curLayoutId, atoi( attr["width"] ),
                atoi( attr["height"] ), atoi( attr["minwidth"] ),
                atoi( attr["maxwidth"] ), atoi( attr["minheight"] ),
                atoi( attr["maxheight"] ), m_curWindowId );
        m_pData->m_listLayout.push_back( layout );
        m_curLayer = 0;
    }

    else if( rName == "Playlist" )
    {
        RequireDefault( "id" );
        RequireDefault( "font" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "width", "0" );
        CheckDefault( "height", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "bgimage", "none" );
        CheckDefault( "fgcolor", "#000000" );
        CheckDefault( "playcolor", "#FF0000" );
        CheckDefault( "bgcolor1", "#FFFFFF" );
        CheckDefault( "bgcolor2", "#FFFFFF" );
        CheckDefault( "selcolor", "#0000FF" );
        CheckDefault( "help", "" );

        m_curListId = uniqueId( attr["id"] );
        const BuilderData::List listData( m_curListId, atoi( attr["x"] ) +
                m_xOffset, atoi( attr["y"] ) + m_yOffset, attr["visible"],
                atoi( attr["width"]), atoi( attr["height"] ),
                attr["lefttop"], attr["rightbottom"], attr["font"],
                "playlist", attr["bgimage"], attr["fgcolor"],
                attr["playcolor"], attr["bgcolor1"], attr["bgcolor2"],
                attr["selcolor"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_pData->m_listList.push_back( listData );
    }

    else if( rName == "Playtree" )
    {
        RequireDefault( "id" );
        RequireDefault( "font" );
        CheckDefault( "visible", "true" );
        CheckDefault( "flat", "false" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "width", "0" );
        CheckDefault( "height", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "bgimage", "none" );
        CheckDefault( "itemimage", "none" );
        CheckDefault( "openimage", "none" );
        CheckDefault( "closedimage", "none" );
        CheckDefault( "fgcolor", "#000000" );
        CheckDefault( "playcolor", "#FF0000" );
        CheckDefault( "bgcolor1", "#FFFFFF" );
        CheckDefault( "bgcolor2", "#FFFFFF" );
        CheckDefault( "selcolor", "#0000FF" );
        CheckDefault( "help", "" );

        m_curTreeId = uniqueId( attr["id"] );
        const BuilderData::Tree treeData( m_curTreeId, atoi( attr["x"] ) +
                m_xOffset, atoi( attr["y"] ) + m_yOffset, attr["visible"],
                attr["flat"],
                atoi( attr["width"]), atoi( attr["height"] ),
                attr["lefttop"], attr["rightbottom"],
                attr["font"], "playtree",
                attr["bgimage"], attr["itemimage"],
                attr["openimage"], attr["closedimage"],
                attr["fgcolor"], attr["playcolor"],
                attr["bgcolor1"], attr["bgcolor2"],
                attr["selcolor"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_pData->m_listTree.push_back( treeData );
    }

    else if( rName == "RadialSlider" )
    {
        RequireDefault( "sequence" );
        RequireDefault( "nbimages" );
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "minangle", "0" );
        CheckDefault( "maxangle", "360" );
        CheckDefault( "value", "none" );
        CheckDefault( "tooltiptext", "" );
        CheckDefault( "help", "" );

        const BuilderData::RadialSlider radial( uniqueId( attr["id"] ),
                attr["visible"],
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["lefttop"], attr["rightbottom"], attr["sequence"],
                atoi( attr["nbImages"] ), atof( attr["minAngle"] ) * M_PI /180,
                atof( attr["maxAngle"] ) * M_PI / 180, attr["value"],
                attr["tooltiptext"], attr["help"], m_curLayer, m_curWindowId,
                m_curLayoutId );
        m_curLayer++;
        m_pData->m_listRadialSlider.push_back( radial );
    }

    else if( rName == "Slider" )
    {
        RequireDefault( "up" );
        RequireDefault( "points" );
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "width", "0" );
        CheckDefault( "height", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "down", "none" );
        CheckDefault( "over", "none" );
        CheckDefault( "thickness", "10" );
        CheckDefault( "value", "none" );
        CheckDefault( "tooltiptext", "" );
        CheckDefault( "help", "" );

        string newValue = attr["value"];
        if( m_curListId != "" )
        {
            // Slider associated to a list
            newValue = "playlist.slider";
        }
        else if( m_curTreeId != "" )
        {
            // Slider associated to a tree
            newValue = "playtree.slider";
        }
        const BuilderData::Slider slider( uniqueId( attr["id"] ),
                attr["visible"], atoi( attr["x"] ) + m_xOffset,
                atoi( attr["y"] ) + m_yOffset, attr["lefttop"],
                attr["rightbottom"], attr["up"], attr["down"],
                attr["over"], attr["points"], atoi( attr["thickness"] ),
                newValue, "none", 0, 0, 0, 0, attr["tooltiptext"],
                attr["help"], m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_pData->m_listSlider.push_back( slider );
    }

    else if( rName == "SliderBackground" )
    {
        RequireDefault( "image" );
        CheckDefault( "nbhoriz", "1" );
        CheckDefault( "nbvert", "1" );
        CheckDefault( "padhoriz", "0" );
        CheckDefault( "padvert", "0" );

        // Retrieve the current slider data
        BuilderData::Slider &slider = m_pData->m_listSlider.back();

        slider.m_imageId = attr["image"];
        slider.m_nbHoriz = atoi( attr["nbhoriz"] );
        slider.m_nbVert = atoi( attr["nbvert"] );
        slider.m_padHoriz = atoi( attr["padhoriz"] );
        slider.m_padVert = atoi( attr["padvert"] );
    }

    else if( rName == "Text" )
    {
        RequireDefault( "font" );
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "text", "" );
        CheckDefault( "color", "#000000" );
        CheckDefault( "scrolling", "auto" );
        CheckDefault( "alignment", "left" );
        CheckDefault( "width", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "help", "" );

        const BuilderData::Text textData( uniqueId( attr["id"] ),
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["visible"], attr["font"],
                attr["text"], atoi( attr["width"] ),
                attr["lefttop"], attr["rightbottom"],
                convertColor( attr["color"] ),
                attr["scrolling"], attr["alignment"],
                attr["help"], m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_pData->m_listText.push_back( textData );
    }

    else if( rName == "Theme" )
    {
        RequireDefault( "version" );
        CheckDefault( "tooltipfont", "defaultfont" );
        CheckDefault( "magnet", "15" );
        CheckDefault( "alpha", "255" );
        CheckDefault( "movealpha", "255" );

        // Check the version
        if( strcmp( attr["version"], SKINS_DTD_VERSION ) )
        {
            msg_Err( getIntf(), "bad theme version : %s (you need version %s)",
                     attr["version"], SKINS_DTD_VERSION );
            m_errors = true;
            return;
        }
        const BuilderData::Theme theme( attr["tooltipfont"],
                atoi( attr["magnet"] ),
                convertInRange( attr["alpha"], 1, 255, "alpha" ),
                convertInRange( attr["movealpha"], 1, 255, "movealpha" ) );
        m_pData->m_listTheme.push_back( theme );
    }

    else if( rName == "ThemeInfo" )
    {
        msg_Info( getIntf(), "skin: %s  author: %s", attr["name"],
                  attr["author"] );
    }

    else if( rName == "Video" )
    {
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "width", "0" );
        CheckDefault( "height", "0" );
        CheckDefault( "lefttop", "lefttop" );
        CheckDefault( "rightbottom", "lefttop" );
        CheckDefault( "autoresize", "false" );
        CheckDefault( "help", "" );

        const BuilderData::Video videoData( uniqueId( attr["id"] ),
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                atoi( attr["width"] ), atoi( attr["height" ]),
                attr["lefttop"], attr["rightbottom"],
                attr["visible"], convertBoolean( attr["autoresize"] ),
                attr["help"], m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_pData->m_listVideo.push_back( videoData );
    }

    else if( rName == "Window" )
    {
        CheckDefault( "id", "none" );
        CheckDefault( "visible", "true" );
        CheckDefault( "x", "0" );
        CheckDefault( "y", "0" );
        CheckDefault( "dragdrop", "true" );
        CheckDefault( "playondrop", "true" );

        m_curWindowId = uniqueId( attr["id"] );
        const BuilderData::Window window( m_curWindowId,
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                convertBoolean( attr["visible"] ),
                convertBoolean( attr["dragdrop"] ),
                convertBoolean( attr["playondrop"] ) );
        m_pData->m_listWindow.push_back( window );
    }
}


void SkinParser::handleEndElement( const string &rName )
{
    if( rName == "Group" )
    {
        m_xOffset -= m_xOffsetList.back();
        m_yOffset -= m_yOffsetList.back();
        m_xOffsetList.pop_back();
        m_yOffsetList.pop_back();
    }
    else if( rName == "Playlist" )
    {
        m_curListId = "";
    }
    else if( rName == "Playtree" )
    {
        m_curTreeId = "";
    }
    else if( rName == "Popup" )
    {
        m_curPopupId = "";
        m_popupPosList.pop_back();
    }
}


bool SkinParser::convertBoolean( const char *value ) const
{
    return strcmp( value, "true" ) == 0;
}


int SkinParser::convertColor( const char *transcolor )
{
    // TODO: move to the builder
    unsigned long iRed, iGreen, iBlue;
    iRed = iGreen = iBlue = 0;
    sscanf( transcolor, "#%2lX%2lX%2lX", &iRed, &iGreen, &iBlue );
    return ( iRed << 16 | iGreen << 8 | iBlue );
}


int SkinParser::convertInRange( const char *value, int minValue, int maxValue,
                                const string &rAttribute ) const
{
    int intValue = atoi( value );

    if( intValue < minValue )
    {
        msg_Warn( getIntf(), "value of \"%s\" attribute (%i) is out of the "
                  "expected range [%i, %i], using %i instead",
                  rAttribute.c_str(), intValue, minValue, maxValue, minValue );
        return minValue;
    }
    else if( intValue > maxValue )
    {
        msg_Warn( getIntf(), "value of \"%s\" attribute (%i) is out of the "
                  "expected range [%i, %i], using %i instead",
                  rAttribute.c_str(), intValue, minValue, maxValue, maxValue );
        return maxValue;
    }
    else
    {
        return intValue;
    }
}


const string SkinParser::generateId() const
{
    static int i = 1;

    char genId[5];
    snprintf( genId, 4, "%i", i++ );

    string base = "_ReservedId_" + (string)genId;

    return base;
}


const string SkinParser::uniqueId( const string &id )
{
    string newId;

    if( m_idSet.find( id ) != m_idSet.end() )
    {
        // The id was already used
        if( id != "none" )
        {
            msg_Warn( getIntf(), "non-unique id: %s", id.c_str() );
        }
        newId = generateId();
    }
    else
    {
        // OK, this is a new id
        newId = id;
    }

    // Add the id to the set
    m_idSet.insert( newId );

    return newId;
}
