/*****************************************************************************
 * skin_parser.cpp
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "skin_parser.hpp"
#include "../src/os_factory.hpp"
#include <math.h>
#include <libxml/catalog.h>
#include <sys/stat.h>

// Current DTD version
#define SKINS_DTD_VERSION "2.0"

// Static variable to avoid initializing catalogs twice
bool SkinParser::m_initialized = false;


SkinParser::SkinParser( intf_thread_t *pIntf, const string &rFileName,
                        const string &rPath ):
    XMLParser( pIntf, rFileName ), m_xOffset( 0 ), m_yOffset( 0 ),
    m_path( rPath )
{
    // Avoid duplicate initialization (mutex needed ?)
    if( !m_initialized )
    {
        // Get the resource path and look for the DTD
        OSFactory *pOSFactory = OSFactory::instance( getIntf() );
        const list<string> &resPath = pOSFactory->getResourcePath();
        const string &sep = pOSFactory->getDirSeparator();
        list<string>::const_iterator it;
        struct stat statBuf;

        // Try to load the catalog first (needed at least on win32 where
        // we don't have a default catalog)
        for( it = resPath.begin(); it != resPath.end(); it++ )
        {
            string catalog_path = (*it) + sep + "skin.catalog";
            if( !stat( catalog_path.c_str(), &statBuf ) )
            {
                msg_Dbg( getIntf(), "Using catalog %s", catalog_path.c_str() );
                xmlLoadCatalog( catalog_path.c_str() );
                break;
            }
        }
        if( it == resPath.end() )
        {
            // Ok, try the default one
            xmlInitializeCatalog();
        }

        for( it = resPath.begin(); it != resPath.end(); it++ )
        {
            string path = (*it) + sep + "skin.dtd";
            if( !stat( path.c_str(), &statBuf ) )
            {
                // DTD found
                msg_Dbg( getIntf(), "Using DTD %s", path.c_str() );

                // Add an entry in the default catalog
                xmlCatalogAdd( (xmlChar*)"public",
                               (xmlChar*)("-//VideoLAN//DTD VLC Skins V"
                                          SKINS_DTD_VERSION "//EN"),
                               (xmlChar*)path.c_str() );
                break;
            }
        }
        if( it == resPath.end() )
        {
            msg_Err( getIntf(), "Cannot find the skins DTD !");
        }
        m_initialized = true;
    }
}


void SkinParser::handleBeginElement( const string &rName, AttrList_t &attr )
{
    if( rName == "Anchor" )
    {
        const BuilderData::Anchor anchor( atoi( attr["x"] ) + m_xOffset,
                atoi( attr["y"] ) + m_yOffset, atoi( attr["range"] ),
                atoi( attr["priority"] ), attr["points"], m_curLayoutId );
        m_data.m_listAnchor.push_back( anchor );
    }

    else if( rName == "Bitmap" )
    {
        const BuilderData::Bitmap bitmap( attr["id"] ,
                convertFileName( attr["file"] ),
                convertColor( attr["alphacolor"] ) );
        m_data.m_listBitmap.push_back( bitmap );
    }

    else if( rName == "BitmapFont" )
    {
        const BuilderData::BitmapFont font( attr["id"],
                convertFileName( attr["file"] ),
                attr["type"] );
        m_data.m_listBitmapFont.push_back( font );
    }

    else if( rName == "Button" )
    {
        const BuilderData::Button button( uniqueId( attr["id"] ), atoi( attr["x"] ) +
                m_xOffset, atoi( attr["y"] ) + m_yOffset, attr["lefttop"],
                attr["rightbottom"], attr["visible"], attr["up"], attr["down"],
                attr["over"], attr["action"], attr["tooltiptext"], attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_data.m_listButton.push_back( button );
    }

    else if( rName == "Checkbox" )
    {
        const BuilderData::Checkbox checkbox( uniqueId( attr["id"] ), atoi( attr["x"] ) +
                m_xOffset, atoi( attr["y"] ) + m_yOffset, attr["lefttop"],
                attr["rightbottom"], attr["visible"], attr["up1"], attr["down1"], attr["over1"],
                attr["up2"], attr["down2"], attr["over2"], attr["state"],
                attr["action1"], attr["action2"], attr["tooltiptext1"],
                attr["tooltiptext2"], attr["help"], m_curLayer, m_curWindowId,
                m_curLayoutId );
        m_curLayer++;
        m_data.m_listCheckbox.push_back( checkbox );
    }

    else if( rName == "Font" )
    {
        const BuilderData::Font fontData( attr["id"],
                convertFileName( attr["file"] ),
                atoi( attr["size"] ) );
        m_data.m_listFont.push_back( fontData );
    }

    else if( rName == "Group" )
    {
        m_xOffset += atoi( attr["x"] );
        m_yOffset += atoi( attr["y"] );
        m_xOffsetList.push_back( atoi( attr["x"] ) );
        m_yOffsetList.push_back( atoi( attr["y"] ) );
    }

    else if( rName == "Image" )
    {
        const BuilderData::Image imageData( uniqueId( attr["id"] ), atoi( attr["x"] ) +
                m_xOffset, atoi( attr["y"] ) + m_yOffset, attr["lefttop"],
                attr["rightbottom"], attr["visible"],
                attr["image"], attr["action"], attr["help"], m_curLayer,
                m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_data.m_listImage.push_back( imageData );
    }

    else if( rName == "Layout" )
    {
        m_curLayoutId = uniqueId( attr["id"] );
        const BuilderData::Layout layout( m_curLayoutId, atoi( attr["width"] ),
                atoi( attr["height"] ), atoi( attr["minwidth"] ),
                atoi( attr["maxwidth"] ), atoi( attr["minheight"] ),
                atoi( attr["maxheight"] ), m_curWindowId );
        m_data.m_listLayout.push_back( layout );
        m_curLayer = 0;
    }

    else if( rName == "Playlist" )
    {
        m_curListId = uniqueId( attr["id"] );
        const BuilderData::List listData( m_curListId, atoi( attr["x"] ) +
                m_xOffset, atoi( attr["y"] ) + m_yOffset, attr["visible"],
                atoi( attr["width"]), atoi( attr["height"] ),
                attr["lefttop"], attr["rightbottom"],
                attr["font"], "playlist", convertColor( attr["fgcolor"] ),
                convertColor( attr["playcolor"] ),
                convertColor( attr["bgcolor1"] ),
                convertColor( attr["bgcolor2"] ),
                convertColor( attr["selcolor"] ), attr["help"],
                m_curLayer, m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_data.m_listList.push_back( listData );
    }

    else if( rName == "RadialSlider" )
    {
        const BuilderData::RadialSlider radial( uniqueId( attr["id"] ),
                attr["visible"],
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["lefttop"], attr["rightbottom"], attr["sequence"],
                atoi( attr["nbImages"] ), atof( attr["minAngle"] ) * M_PI / 180,
                atof( attr["maxAngle"] ) * M_PI / 180, attr["value"],
                attr["tooltiptext"], attr["help"], m_curLayer, m_curWindowId,
                m_curLayoutId );
        m_curLayer++;
        m_data.m_listRadialSlider.push_back( radial );
    }

    else if( rName == "Slider" )
    {
        string newValue = attr["value"];
        if( m_curListId != "" )
        {
            // Slider associated to a list
            newValue = "playlist.slider";
        }
        const BuilderData::Slider slider( uniqueId( attr["id"] ),
                attr["visible"],
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["lefttop"], attr["rightbottom"], attr["up"], attr["down"],
                attr["over"], attr["points"], atoi( attr["thickness"] ),
                newValue, attr["tooltiptext"], attr["help"], m_curLayer,
                m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_data.m_listSlider.push_back( slider );
    }

    else if( rName == "Text" )
    {
        const BuilderData::Text textData( uniqueId( attr["id"] ),
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                attr["visible"], attr["font"],
                attr["text"], atoi( attr["width"] ),
                convertColor( attr["color"] ), attr["help"], m_curLayer,
                m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_data.m_listText.push_back( textData );
    }

    else if( rName == "Theme" )
    {
        // Check the version
        if( strcmp( attr["version"], SKINS_DTD_VERSION ) )
        {
            msg_Err( getIntf(), "Bad theme version : %s (you need version %s)",
                     attr["version"], SKINS_DTD_VERSION );
            m_errors = true;
            return;
        }
        const BuilderData::Theme theme( attr["tooltipfont"],
                atoi( attr["magnet"] ),
                convertInRange( attr["alpha"], 1, 255, "alpha" ),
                convertInRange( attr["movealpha"], 1, 255, "movealpha" ),
                atoi( attr["fadetime"] ) );
        m_data.m_listTheme.push_back( theme );
    }

    else if( rName == "ThemeInfo" )
    {
        msg_Info( getIntf(), "skin: %s  author: %s", attr["name"],
                  attr["author"] );
    }

    else if( rName == "Video" )
    {
        const BuilderData::Video videoData( uniqueId( attr["id"] ),
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                atoi( attr["width"] ), atoi( attr["height" ]),
                attr["lefttop"], attr["rightbottom"],
                attr["visible"], attr["help"], m_curLayer,
                m_curWindowId, m_curLayoutId );
        m_curLayer++;
        m_data.m_listVideo.push_back( videoData );
    }

    else if( rName == "Window" )
    {
        m_curWindowId = uniqueId( attr["id"] );
        const BuilderData::Window window( m_curWindowId,
                atoi( attr["x"] ) + m_xOffset, atoi( attr["y"] ) + m_yOffset,
                convertBoolean( attr["visible"] ),
                convertBoolean( attr["dragdrop"] ),
                convertBoolean( attr["playondrop"] ) );
        m_data.m_listWindow.push_back( window );
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
}


bool SkinParser::convertBoolean( const char *value ) const
{
    return strcmp( value, "true" ) == 0;
}


int SkinParser::convertColor( const char *transcolor ) const
{
    unsigned long iRed, iGreen, iBlue;
    iRed = iGreen = iBlue = 0;
    sscanf( transcolor, "#%2lX%2lX%2lX", &iRed, &iGreen, &iBlue );
    return ( iRed << 16 | iGreen << 8 | iBlue );
}


string SkinParser::convertFileName( const char *fileName ) const
{
    return m_path + string( fileName );
}


int SkinParser::convertInRange( const char *value, int minValue, int maxValue,
                                const string &rAttribute ) const
{
    int intValue = atoi( value );

    if( intValue < minValue )
    {
        msg_Warn( getIntf(), "Value of \"%s\" attribute (%i) is out of the "
                  "expected range [%i, %i], using %i instead",
                  rAttribute.c_str(), intValue, minValue, maxValue, minValue );
        return minValue;
    }
    else if( intValue > maxValue )
    {
        msg_Warn( getIntf(), "Value of \"%s\" attribute (%i) is out of the "
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
            msg_Warn( getIntf(), "Non unique id: %s", id.c_str() );
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

