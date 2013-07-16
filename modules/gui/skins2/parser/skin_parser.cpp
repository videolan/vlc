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
                        const string &rPath, BuilderData *pData ):
    XMLParser( pIntf, rFileName ), m_path( rPath ), m_pData( pData ),
    m_ownData( pData == NULL ), m_xOffset( 0 ), m_yOffset( 0 )
{
    // Make sure the data is allocated
    if( m_pData == NULL )
    {
        m_pData = new BuilderData();
    }

    // Special id, we don't want any control to have the same one
    m_idSet.insert( "none" );
    // At the beginning, there is no Panel
    m_panelStack.push_back( "none" );
}


SkinParser::~SkinParser()
{
    if( m_ownData )
    {
        delete m_pData;
    }
}

inline bool SkinParser::MissingAttr( AttrList_t &attr, const string &name,
                                     const char *a )
{
    if( attr.find(a) == attr.end() )
    {
        msg_Err( getIntf(), "bad theme (element: %s, missing attribute: %s)",
                 name.c_str(), a );
        m_errors = true; return true;
    }
    return false;
}

void SkinParser::handleBeginElement( const string &rName, AttrList_t &attr )
{
#define RequireAttr( attr, name, a ) \
    if( MissingAttr( attr, name, a ) ) return;

    if( rName == "Include" )
    {
        RequireAttr( attr, rName, "file" );

        OSFactory *pFactory = OSFactory::instance( getIntf() );
        string fullPath = m_path + pFactory->getDirSeparator() + attr["file"];
        msg_Dbg( getIntf(), "opening included XML file: %s", fullPath.c_str() );
        SkinParser subParser( getIntf(), fullPath.c_str(), m_path, m_pData );
        subParser.parse();
    }

    else if( rName == "IniFile" )
    {
        RequireAttr( attr, rName, "id" );
        RequireAttr( attr, rName, "file" );

        const BuilderData::IniFile iniFile( uniqueId( attr["id"] ),
                attr["file"] );
        m_pData->m_listIniFile.push_back( iniFile );
    }

    else if( rName == "Anchor" )
    {
        RequireAttr( attr, rName, "priority" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "points", "(0,0)" );
        DefaultAttr( attr, "range", "10" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        const BuilderData::Anchor anchor( x + m_xOffset,
                y + m_yOffset, attr["lefttop"],
                atoi( attr["range"] ), atoi( attr["priority"] ),
                attr["points"], m_curLayoutId );
        m_pData->m_listAnchor.push_back( anchor );
    }

    else if( rName == "Bitmap" )
    {
        RequireAttr( attr, rName, "id" );
        RequireAttr( attr, rName, "file" );
        RequireAttr( attr, rName, "alphacolor" );
        DefaultAttr( attr, "nbframes", "1" );
        DefaultAttr( attr, "fps", "4" );
        DefaultAttr( attr, "loop", "0" );

        m_curBitmapId = uniqueId( attr["id"] );
        const BuilderData::Bitmap bitmap( m_curBitmapId,
                attr["file"], convertColor( attr["alphacolor"] ),
                atoi( attr["nbframes"] ), atoi( attr["fps"] ),
                atoi( attr["loop"] ) );
        m_pData->m_listBitmap.push_back( bitmap );
    }

    else if( rName == "SubBitmap" )
    {
        RequireAttr( attr, rName, "id" );
        RequireAttr( attr, rName, "x" );
        RequireAttr( attr, rName, "y" );
        RequireAttr( attr, rName, "width" );
        RequireAttr( attr, rName, "height" );
        DefaultAttr( attr, "nbframes", "1" );
        DefaultAttr( attr, "fps", "4" );
        DefaultAttr( attr, "loop", "0" );

        const BuilderData::SubBitmap bitmap( uniqueId( attr["id"] ),
                m_curBitmapId, atoi( attr["x"] ), atoi( attr["y"] ),
                atoi( attr["width"] ), atoi( attr["height"] ),
                atoi( attr["nbframes"] ), atoi( attr["fps"] ),
                atoi( attr["loop"] ) );
        m_pData->m_listSubBitmap.push_back( bitmap );
    }

    else if( rName == "BitmapFont" )
    {
        RequireAttr( attr, rName, "id" );
        RequireAttr( attr, rName, "file" );
        DefaultAttr( attr, "type", "digits" );

        const BuilderData::BitmapFont font( uniqueId( attr["id"] ),
                attr["file"], attr["type"] );
        m_pData->m_listBitmapFont.push_back( font );
    }

    else if( rName == "PopupMenu" )
    {
        RequireAttr( attr, rName, "id" );

        m_popupPosList.push_back(0);
        m_curPopupId = uniqueId( attr["id"] );
        const BuilderData::PopupMenu popup( m_curPopupId );
        m_pData->m_listPopupMenu.push_back( popup );
    }

    else if( rName == "MenuItem" )
    {
        RequireAttr( attr, rName, "label" );
        DefaultAttr( attr, "action", "none" );

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
        RequireAttr( attr, rName, "up" );
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "down", "none" );
        DefaultAttr( attr, "over", "none" );
        DefaultAttr( attr, "action", "none" );
        DefaultAttr( attr, "tooltiptext", "" );
        DefaultAttr( attr, "help", "" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        const BuilderData::Button button( uniqueId( attr["id"] ),
                x + m_xOffset, y + m_yOffset,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ), attr["visible"],
                attr["up"], attr["down"], attr["over"], attr["action"],
                attr["tooltiptext"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId, m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listButton.push_back( button );
    }

    else if( rName == "Checkbox" )
    {
        RequireAttr( attr, rName, "up1" );
        RequireAttr( attr, rName, "up2" );
        RequireAttr( attr, rName, "state" );
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "down1", "none" );
        DefaultAttr( attr, "over1", "none" );
        DefaultAttr( attr, "down2", "none" );
        DefaultAttr( attr, "over2", "none" );
        DefaultAttr( attr, "action1", "none" );
        DefaultAttr( attr, "action2", "none" );
        DefaultAttr( attr, "tooltiptext1", "" );
        DefaultAttr( attr, "tooltiptext2", "" );
        DefaultAttr( attr, "help", "" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        const BuilderData::Checkbox checkbox( uniqueId( attr["id"] ),
                x + m_xOffset, y + m_yOffset,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ), attr["visible"],
                attr["up1"], attr["down1"], attr["over1"],
                attr["up2"], attr["down2"], attr["over2"], attr["state"],
                attr["action1"], attr["action2"], attr["tooltiptext1"],
                attr["tooltiptext2"], attr["help"], m_curLayer, m_curWindowId,
                m_curLayoutId, m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listCheckbox.push_back( checkbox );
    }

    else if( rName == "Font" )
    {
        RequireAttr( attr, rName, "id" );
        RequireAttr( attr, rName, "file" );
        DefaultAttr( attr, "size", "12" );

        const BuilderData::Font fontData( uniqueId( attr["id"] ),
                attr["file"], atoi( attr["size"] ) );
        m_pData->m_listFont.push_back( fontData );
    }

    else if( rName == "Group" )
    {
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );

        m_xOffset += atoi( attr["x"] );
        m_yOffset += atoi( attr["y"] );
        m_xOffsetList.push_back( atoi( attr["x"] ) );
        m_yOffsetList.push_back( atoi( attr["y"] ) );
    }

    else if( rName == "Image" )
    {
        RequireAttr( attr, rName, "image" );
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "width", "-1" );
        DefaultAttr( attr, "height", "-1" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "action", "none" );
        DefaultAttr( attr, "action2", "none" );
        DefaultAttr( attr, "resize", "mosaic" );
        DefaultAttr( attr, "help", "" );
        DefaultAttr( attr, "art", "false" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        int width = getDimension( attr["width"], refWidth );
        int height = getDimension( attr["height"], refHeight );
        const BuilderData::Image imageData( uniqueId( attr["id"] ),
                x + m_xOffset, y + m_yOffset, width, height,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ), attr["visible"],
                attr["image"], attr["action"], attr["action2"], attr["resize"],
                attr["help"], convertBoolean( attr["art"] ),
                m_curLayer, m_curWindowId, m_curLayoutId,
                m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listImage.push_back( imageData );
    }

    else if( rName == "Layout" )
    {
        RequireAttr( attr, rName, "width" );
        RequireAttr( attr, rName, "height" );
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "minwidth", "-1" );
        DefaultAttr( attr, "maxwidth", "-1" );
        DefaultAttr( attr, "minheight", "-1" );
        DefaultAttr( attr, "maxheight", "-1" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, true );
        int width = getDimension( attr["width"], refWidth );
        int height = getDimension( attr["height"], refHeight );

        m_curLayoutId = uniqueId( attr["id"] );
        const BuilderData::Layout layout( m_curLayoutId,
                width, height,
                getDimension( attr["minwidth"], refWidth ),
                getDimension( attr["maxwidth"], refWidth ),
                getDimension( attr["minheight"], refHeight ),
                getDimension( attr["maxheight"], refHeight ),
                m_curWindowId );

        updateWindowPos( width, height );
        m_pData->m_listLayout.push_back( layout );
        m_curLayer = 0;
    }

    else if( rName == "Panel" )
    {
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        RequireAttr( attr, rName, "width" );
        RequireAttr( attr, rName, "height" );
        DefaultAttr( attr, "position", "-1" );
        DefaultAttr( attr, "xoffset", "0" );
        DefaultAttr( attr, "yoffset", "0" );
        DefaultAttr( attr, "xmargin", "0" );
        DefaultAttr( attr, "ymargin", "0" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        int width = getDimension( attr["width"], refWidth );
        int height = getDimension( attr["height"], refHeight );
        convertPosition( attr["position"],
                         attr["xoffset"], attr["yoffset"],
                         attr["xmargin"], attr["ymargin"],
                         width, height, refWidth, refHeight, &x, &y );

        string panelId = uniqueId( "none" );
        const BuilderData::Panel panel( panelId,
                x + m_xOffset, y + m_yOffset,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ),
                width, height,
                m_curLayer, m_curWindowId, m_curLayoutId, m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listPanel.push_back( panel );
        // Add the panel to the stack
        m_panelStack.push_back( panelId );
    }

    else if( rName == "Playlist" )
    {
        RequireAttr( attr, rName, "id" );
        RequireAttr( attr, rName, "font" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "flat", "true" ); // Only difference here
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "width", "0" );
        DefaultAttr( attr, "height", "0" );
        DefaultAttr( attr, "position", "-1" );
        DefaultAttr( attr, "xoffset", "0" );
        DefaultAttr( attr, "yoffset", "0" );
        DefaultAttr( attr, "xmargin", "0" );
        DefaultAttr( attr, "ymargin", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "bgimage", "none" );
        DefaultAttr( attr, "itemimage", "none" );
        DefaultAttr( attr, "openimage", "none" );
        DefaultAttr( attr, "closedimage", "none" );
        DefaultAttr( attr, "fgcolor", "#000000" );
        DefaultAttr( attr, "playcolor", "#FF0000" );
        DefaultAttr( attr, "bgcolor1", "#FFFFFF" );
        DefaultAttr( attr, "bgcolor2", "#FFFFFF" );
        DefaultAttr( attr, "selcolor", "#0000FF" );
        DefaultAttr( attr, "help", "" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        int width = getDimension( attr["width"], refWidth );
        int height = getDimension( attr["height"], refHeight );
        convertPosition( attr["position"],
                         attr["xoffset"], attr["yoffset"],
                         attr["xmargin"], attr["ymargin"],
                         width, height, refWidth, refHeight, &x, &y );

        m_curTreeId = uniqueId( attr["id"] );
        const BuilderData::Tree treeData( m_curTreeId,
                x + m_xOffset, y + m_yOffset, attr["visible"],
                attr["flat"],
                width, height,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ),
                attr["font"], "playtree",
                attr["bgimage"], attr["itemimage"],
                attr["openimage"], attr["closedimage"],
                attr["fgcolor"],
                attr["playcolor"],
                attr["bgcolor1"],
                attr["bgcolor2"],
                attr["selcolor"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId, m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listTree.push_back( treeData );
    }
    else if( rName == "Playtree" )
    {
        RequireAttr( attr, rName, "id" );
        RequireAttr( attr, rName, "font" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "flat", "false" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "width", "0" );
        DefaultAttr( attr, "height", "0" );
        DefaultAttr( attr, "position", "-1" );
        DefaultAttr( attr, "xoffset", "0" );
        DefaultAttr( attr, "yoffset", "0" );
        DefaultAttr( attr, "xmargin", "0" );
        DefaultAttr( attr, "ymargin", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "bgimage", "none" );
        DefaultAttr( attr, "itemimage", "none" );
        DefaultAttr( attr, "openimage", "none" );
        DefaultAttr( attr, "closedimage", "none" );
        DefaultAttr( attr, "fgcolor", "#000000" );
        DefaultAttr( attr, "playcolor", "#FF0000" );
        DefaultAttr( attr, "bgcolor1", "#FFFFFF" );
        DefaultAttr( attr, "bgcolor2", "#FFFFFF" );
        DefaultAttr( attr, "selcolor", "#0000FF" );
        DefaultAttr( attr, "help", "" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        int width = getDimension( attr["width"], refWidth );
        int height = getDimension( attr["height"], refHeight );
        convertPosition( attr["position"],
                         attr["xoffset"], attr["yoffset"],
                         attr["xmargin"], attr["ymargin"],
                         width, height, refWidth, refHeight, &x, &y );

        m_curTreeId = uniqueId( attr["id"] );
        const BuilderData::Tree treeData( m_curTreeId,
                x + m_xOffset, y + m_yOffset, attr["visible"],
                attr["flat"],
                width, height,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ),
                attr["font"], "playtree",
                attr["bgimage"], attr["itemimage"],
                attr["openimage"], attr["closedimage"],
                attr["fgcolor"], attr["playcolor"],
                attr["bgcolor1"], attr["bgcolor2"],
                attr["selcolor"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId, m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listTree.push_back( treeData );
    }

    else if( rName == "RadialSlider" )
    {
        RequireAttr( attr, rName, "sequence" );
        RequireAttr( attr, rName, "nbimages" );
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "minangle", "0" );
        DefaultAttr( attr, "maxangle", "360" );
        DefaultAttr( attr, "value", "none" );
        DefaultAttr( attr, "tooltiptext", "" );
        DefaultAttr( attr, "help", "" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        const BuilderData::RadialSlider radial( uniqueId( attr["id"] ),
                attr["visible"],
                x + m_xOffset, y + m_yOffset,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ), attr["sequence"],
                atoi( attr["nbimages"] ), atof( attr["minangle"] ) * M_PI /180,
                atof( attr["maxangle"] ) * M_PI / 180, attr["value"],
                attr["tooltiptext"], attr["help"], m_curLayer, m_curWindowId,
                m_curLayoutId, m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listRadialSlider.push_back( radial );
    }

    else if( rName == "Slider" )
    {
        RequireAttr( attr, rName, "up" );
        RequireAttr( attr, rName, "points" );
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "width", "-1" );
        DefaultAttr( attr, "height", "-1" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "down", "none" );
        DefaultAttr( attr, "over", "none" );
        DefaultAttr( attr, "thickness", "10" );
        DefaultAttr( attr, "value", "none" );
        DefaultAttr( attr, "tooltiptext", "" );
        DefaultAttr( attr, "help", "" );

        string newValue = attr["value"];
        if( m_curTreeId != "" )
        {
            // Slider associated to a tree
            newValue = "playtree.slider";
        }

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        int width = getDimension( attr["width"], refWidth );
        int height = getDimension( attr["height"], refHeight );
        const BuilderData::Slider slider( uniqueId( attr["id"] ),
                attr["visible"], x + m_xOffset, y + m_yOffset,
                width, height, attr["lefttop"],
                attr["rightbottom"], convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ), attr["up"], attr["down"],
                attr["over"], attr["points"], atoi( attr["thickness"] ),
                newValue, "none", 0, 0, 0, 0, attr["tooltiptext"],
                attr["help"], m_curLayer, m_curWindowId, m_curLayoutId,
                m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listSlider.push_back( slider );
    }

    else if( rName == "SliderBackground" )
    {
        RequireAttr( attr, rName, "image" );
        DefaultAttr( attr, "nbhoriz", "1" );
        DefaultAttr( attr, "nbvert", "1" );
        DefaultAttr( attr, "padhoriz", "0" );
        DefaultAttr( attr, "padvert", "0" );

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
        RequireAttr( attr, rName, "font" );
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "text", "" );
        DefaultAttr( attr, "color", "#000000" );
        DefaultAttr( attr, "scrolling", "auto" );
        DefaultAttr( attr, "alignment", "left" );
        DefaultAttr( attr, "focus", "true" );
        DefaultAttr( attr, "width", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "help", "" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        int width = getDimension( attr["width"], refWidth );

        const BuilderData::Text textData( uniqueId( attr["id"] ),
                x + m_xOffset, y + m_yOffset,
                attr["visible"], attr["font"],
                attr["text"],
                width,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ),
                convertColor( attr["color"] ),
                attr["scrolling"], attr["alignment"],
                attr["focus"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId,
                m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listText.push_back( textData );
    }

    else if( rName == "Theme" )
    {
        RequireAttr( attr, rName, "version" );
        DefaultAttr( attr, "tooltipfont", "defaultfont" );
        DefaultAttr( attr, "magnet", "15" );
        DefaultAttr( attr, "alpha", "255" );
        DefaultAttr( attr, "movealpha", "255" );

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
        DefaultAttr( attr, "name", "" );
        DefaultAttr( attr, "author", "" );
        DefaultAttr( attr, "email", "" );
        DefaultAttr( attr, "website", "" );
        msg_Info( getIntf(), "skin: %s  author: %s", attr["name"],
                  attr["author"] );
    }

    else if( rName == "Video" )
    {
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "width", "0" );
        DefaultAttr( attr, "height", "0" );
        DefaultAttr( attr, "position", "-1" );
        DefaultAttr( attr, "xoffset", "0" );
        DefaultAttr( attr, "yoffset", "0" );
        DefaultAttr( attr, "xmargin", "0" );
        DefaultAttr( attr, "ymargin", "0" );
        DefaultAttr( attr, "lefttop", "lefttop" );
        DefaultAttr( attr, "rightbottom", "lefttop" );
        DefaultAttr( attr, "xkeepratio", "false" );
        DefaultAttr( attr, "ykeepratio", "false" );
        DefaultAttr( attr, "autoresize", "true" );
        DefaultAttr( attr, "help", "" );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, false );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        int width = getDimension( attr["width"], refWidth );
        int height = getDimension( attr["height"], refHeight );
        convertPosition( attr["position"],
                         attr["xoffset"], attr["yoffset"],
                         attr["xmargin"], attr["ymargin"],
                         width, height, refWidth, refHeight, &x, &y );

        const BuilderData::Video videoData( uniqueId( attr["id"] ),
                x + m_xOffset, y + m_yOffset, width, height,
                attr["lefttop"], attr["rightbottom"],
                convertBoolean( attr["xkeepratio"] ),
                convertBoolean( attr["ykeepratio"] ),
                attr["visible"], convertBoolean( attr["autoresize"] ),
                attr["help"], m_curLayer, m_curWindowId, m_curLayoutId,
                m_panelStack.back() );
        m_curLayer++;
        m_pData->m_listVideo.push_back( videoData );
    }

    else if( rName == "Window" )
    {
        DefaultAttr( attr, "id", "none" );
        DefaultAttr( attr, "visible", "true" );
        DefaultAttr( attr, "x", "0" );
        DefaultAttr( attr, "y", "0" );
        DefaultAttr( attr, "position", "-1" );
        DefaultAttr( attr, "xoffset", "0" );
        DefaultAttr( attr, "yoffset", "0" );
        DefaultAttr( attr, "xmargin", "0" );
        DefaultAttr( attr, "ymargin", "0" );
        DefaultAttr( attr, "dragdrop", "true" );
        DefaultAttr( attr, "playondrop", "true" );

        m_curWindowId = uniqueId( attr["id"] );

        int refWidth, refHeight;
        getRefDimensions( refWidth, refHeight, true );
        int x = getDimension( attr["x"], refWidth );
        int y = getDimension( attr["y"], refHeight );
        const BuilderData::Window window( m_curWindowId,
                x + m_xOffset, y + m_yOffset,
                attr["position"],
                attr["xoffset"], attr["yoffset"],
                attr["xmargin"], attr["ymargin"],
                convertBoolean( attr["visible"] ),
                convertBoolean( attr["dragdrop"] ),
                convertBoolean( attr["playondrop"] ) );
        m_pData->m_listWindow.push_back( window );
    }
#undef  RequireAttr
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
    else if( rName == "Playtree" || rName == "Playlist" )
    {
        m_curTreeId = "";
    }
    else if( rName == "Popup" )
    {
        m_curPopupId = "";
        m_popupPosList.pop_back();
    }
    else if( rName == "Panel" )
    {
        m_panelStack.pop_back();
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

void SkinParser::getRefDimensions( int &rWidth, int &rHeight, bool toScreen )
{
    if( toScreen )
    {
        OSFactory *pOsFactory = OSFactory::instance( getIntf() );
        rWidth = pOsFactory->getScreenWidth();
        rHeight = pOsFactory->getScreenHeight();
        return;
    }

    string panelId = m_panelStack.back();
    if( panelId != "none" )
    {
        list<BuilderData::Panel>::const_iterator it;
        for( it = m_pData->m_listPanel.begin();
             it != m_pData->m_listPanel.end(); ++it )
        {
            if( it->m_id == panelId )
            {
                rWidth = it->m_width;
                rHeight = it->m_height;
                return;
            }
        }
    }
    else
    {
        const BuilderData::Layout layout = m_pData->m_listLayout.back();
        rWidth = layout.m_width;
        rHeight = layout.m_height;
        return;
    }
    msg_Err( getIntf(), "failure to retrieve parent panel or layout" );
}


int SkinParser::getDimension( string value, int refDimension )
{
    string::size_type leftPos;

    leftPos = value.find( "%" );
    if( leftPos != string::npos )
    {
        int val = atoi( value.substr( 0, leftPos ).c_str() );
        return  val * refDimension / 100;
    }

    leftPos = value.find( "px" );
    if( leftPos != string::npos )
    {
        int val = atoi( value.substr( 0, leftPos ).c_str() );
        return val;
    }

    return atoi( value.c_str() );
}


int SkinParser::getPosition( string position )
{
    if( position == "-1" )
        return POS_UNDEF;
    else if( position == "Center" )
        return POS_CENTER;
    else if( position == "North" )
        return POS_TOP;
    else if( position == "South" )
        return POS_BOTTOM;
    else if( position == "West" )
        return POS_LEFT;
    else if( position == "East" )
        return POS_RIGHT;
    else if( position == "NorthWest" )
        return POS_TOP | POS_LEFT;
    else if( position == "NorthEast" )
        return POS_TOP | POS_RIGHT;
    else if( position == "SouthWest" )
        return POS_BOTTOM | POS_LEFT;
    else if( position == "SouthEast" )
        return POS_BOTTOM | POS_RIGHT;

    msg_Err( getIntf(), "unknown value '%s' for position",
                          position.c_str() );
    return POS_UNDEF;
}


void SkinParser::convertPosition( string position, string xOffset,
                          string yOffset, string xMargin,
                          string yMargin, int width, int height,
                          int refWidth, int refHeight, int* p_x, int* p_y )
{
    int iPosition = getPosition( position );
    if( iPosition != POS_UNDEF )
    {
        // compute offset against the parent object size
        // for backward compatibility
        int i_xOffset = getDimension( xOffset, refWidth );
        int i_yOffset = getDimension( yOffset, refHeight );
        int i_xMargin = getDimension( xMargin, refWidth );
        int i_yMargin = getDimension( yMargin, refHeight );

        // compute *p_x
        if( iPosition & POS_LEFT )
            *p_x = i_xMargin;
        else if( iPosition & POS_RIGHT )
            *p_x = refWidth - width - i_xMargin;
        else
            *p_x = ( refWidth - width ) / 2;

        // compute *p_y
        if( iPosition & POS_TOP )
            *p_y = i_yMargin;
        else if( iPosition & POS_BOTTOM )
            *p_y = refHeight - height - i_yMargin;
        else
            *p_y = ( refHeight - height ) / 2;

        // add offset
        *p_x += i_xOffset;
        *p_y += i_yOffset;
    }
    else
    {
        // compute offset against the current object size
        int i_xOffset = getDimension( xOffset, width );
        int i_yOffset = getDimension( yOffset, height );

        // add offset
        *p_x += i_xOffset;
        *p_y += i_yOffset;
    }
}


void SkinParser::updateWindowPos( int width, int height )
{
    BuilderData::Window win = m_pData->m_listWindow.back();
    m_pData->m_listWindow.pop_back();

    int refWidth, refHeight;
    getRefDimensions( refWidth, refHeight, true );
    convertPosition( win.m_position,
                     win.m_xOffset, win.m_yOffset,
                     win.m_xMargin, win.m_yMargin,
                     width, height, refWidth, refHeight,
                     &win.m_xPos, &win.m_yPos );

    m_pData->m_listWindow.push_back( win );
}
