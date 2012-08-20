/*****************************************************************************
 * xmlparser.cpp
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

#include "xmlparser.hpp"
#include "../src/os_factory.hpp"
#include <vlc_url.h>

#include <sys/stat.h>

XMLParser::XMLParser( intf_thread_t *pIntf, const string &rFileName )
    : SkinObject( pIntf ), m_pXML( NULL ), m_pReader( NULL ), m_pStream( NULL )
{
    m_pXML = xml_Create( pIntf );
    if( !m_pXML )
    {
        msg_Err( getIntf(), "cannot initialize xml" );
        return;
    }

    LoadCatalog();

    char *psz_uri = vlc_path2uri( rFileName.c_str(), NULL );
    m_pStream = stream_UrlNew( pIntf, psz_uri );
    free( psz_uri );
    if( !m_pStream )
    {
        msg_Err( getIntf(), "failed to open %s for reading",
                 rFileName.c_str() );
        return;
    }

    m_pReader = xml_ReaderCreate( m_pXML, m_pStream );
    if( !m_pReader )
    {
        msg_Err( getIntf(), "failed to open %s for parsing",
                 rFileName.c_str() );
        return;
    }

    xml_ReaderUseDTD( m_pReader );
}


XMLParser::~XMLParser()
{
    if( m_pReader ) xml_ReaderDelete( m_pReader );
    if( m_pXML ) xml_Delete( m_pXML );
    if( m_pStream ) stream_Delete( m_pStream );
}


void XMLParser::LoadCatalog()
{
    // Get the resource path and look for the DTD
    OSFactory *pOSFactory = OSFactory::instance( getIntf() );
    const list<string> &resPath = pOSFactory->getResourcePath();
    const string &sep = pOSFactory->getDirSeparator();
    list<string>::const_iterator it;

    struct stat statBuf;

    // Try to load the catalog first (needed at least on win32 where
    // we don't have a default catalog)
    for( it = resPath.begin(); it != resPath.end(); ++it )
    {
        string catalog_path = (*it) + sep + "skin.catalog";
        if( !stat( catalog_path.c_str(), &statBuf ) )
        {
            msg_Dbg( getIntf(), "Using catalog %s", catalog_path.c_str() );
            xml_CatalogLoad( m_pXML, catalog_path.c_str() );
            break;
        }
    }
    if( it == resPath.end() )
    {
        // Ok, try the default one
        xml_CatalogLoad( m_pXML, NULL );
    }

    for( it = resPath.begin(); it != resPath.end(); ++it )
    {
        string path = (*it) + sep + "skin.dtd";
        if( !stat( path.c_str(), &statBuf ) )
        {
            // DTD found
            msg_Dbg( getIntf(), "using DTD %s", path.c_str() );

            // Add an entry in the default catalog
            xml_CatalogAdd( m_pXML, "public",
                            "-//VideoLAN//DTD VLC Skins V"
                            SKINS_DTD_VERSION "//EN", path.c_str() );
            break;
        }
    }
    if( it == resPath.end() )
    {
        msg_Err( getIntf(), "cannot find the skins DTD");
    }
}

bool XMLParser::parse()
{
    const char *node;
    int type;

    if( !m_pReader ) return false;

    m_errors = false;

    while( (type = xml_ReaderNextNode( m_pReader, &node )) > 0 )
    {
        if( m_errors ) return false;

        switch( type )
        {
            case XML_READER_STARTELEM:
            {
                // Read the attributes
                AttrList_t attributes;
                const char *name, *value;
                while( (name = xml_ReaderNextAttr( m_pReader, &value )) != NULL )
                    attributes[strdup(name)] = strdup(value);

                handleBeginElement( node, attributes );

                map<const char*, const char*, ltstr> ::iterator it =
                    attributes.begin();
                while( it != attributes.end() )
                {
                    free( (char *)it->first );
                    free( (char *)it->second );
                    ++it;
                }
                break;
            }

            // End element
            case XML_READER_ENDELEM:
            {
                handleEndElement( node );
                break;
            }
        }
    }
    return (type == 0 && !m_errors );
}
