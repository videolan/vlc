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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "xmlparser.hpp"
#include "../src/os_factory.hpp"

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

// Static variable to avoid initializing catalogs twice
static bool m_initialized = false;

XMLParser::XMLParser( intf_thread_t *pIntf, const string &rFileName ):
    SkinObject( pIntf )
{
    m_pReader = NULL;
    m_pStream = NULL;
    
    m_pXML = xml_Create( pIntf );
    if( !m_pXML )
    {
        msg_Err( getIntf(), "Failed to open XML parser" );
        return;
    }

    // Avoid duplicate initialization (mutex needed ?)
    if( !m_initialized )
    {
        LoadCatalog();
        m_initialized = true;
    }

    m_pStream = stream_UrlNew( pIntf, rFileName.c_str() );
    if( !m_pStream )
    {
        msg_Err( getIntf(), "Failed to open %s for reading",
                 rFileName.c_str() );
        return;
    }
    m_pReader = xml_ReaderCreate( m_pXML, m_pStream );
    if( !m_pReader )
    {
        msg_Err( getIntf(), "Failed to open %s for parsing",
                 rFileName.c_str() );
        return;
    }

    xml_ReaderUseDTD( m_pReader, VLC_TRUE );

}


XMLParser::~XMLParser()
{
    if( m_pReader && m_pXML ) xml_ReaderDelete( m_pXML, m_pReader );
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

#ifdef HAVE_SYS_STAT_H
    struct stat statBuf;

    // Try to load the catalog first (needed at least on win32 where
    // we don't have a default catalog)
    for( it = resPath.begin(); it != resPath.end(); it++ )
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
        xml_CatalogLoad( m_pXML, 0 );
    }

    for( it = resPath.begin(); it != resPath.end(); it++ )
    {
        string path = (*it) + sep + "skin.dtd";
        if( !stat( path.c_str(), &statBuf ) )
        {
            // DTD found
            msg_Dbg( getIntf(), "Using DTD %s", path.c_str() );

            // Add an entry in the default catalog
            xml_CatalogAdd( m_pXML, "public",
                            "-//VideoLAN//DTD VLC Skins V"
                            SKINS_DTD_VERSION "//EN", path.c_str() );
            break;
        }
    }
    if( it == resPath.end() )
    {
        msg_Err( getIntf(), "Cannot find the skins DTD !");
    }
#endif
}

bool XMLParser::parse()
{
    if( !m_pReader ) return false;

    m_errors = false;

    int ret = xml_ReaderRead( m_pReader );
    while( ret == 1 )
    {
        if( m_errors ) return false;

        // Get the node type
        int type = xml_ReaderNodeType( m_pReader );
        switch( type )
        {
            // Error
            case -1:
                return false;
                break;

            case XML_READER_STARTELEM:
            {
                // Read the element name
                char *eltName = xml_ReaderName( m_pReader );
                if( !eltName ) return false;

                // Read the attributes
                AttrList_t attributes;
                while( xml_ReaderNextAttr( m_pReader ) == VLC_SUCCESS )
                {
                    char *name = xml_ReaderName( m_pReader );
                    char *value = xml_ReaderValue( m_pReader );
                    if( !name || !value ) return false;
                    attributes[name] = value;
                }

                handleBeginElement( eltName, attributes );
                free( eltName );

                map<const char*, const char*, ltstr> ::iterator it =
                    attributes.begin();
                while( it != attributes.end() )
                {
                    free( (char *)it->first );
                    free( (char *)it->second );
                    it++;
                }
                break;
            }

            // End element
            case XML_READER_ENDELEM:
            {
                // Read the element name
                char *eltName = xml_ReaderName( m_pReader );
                if( !eltName ) return false;

                handleEndElement( eltName );
                free( eltName );
                break;
            }
        }
        ret = xml_ReaderRead( m_pReader );
    }
    return (ret == 0 && !m_errors );
}
