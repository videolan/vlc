/*****************************************************************************
 * xmlparser.cpp
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

#include "xmlparser.hpp"

XMLParser::XMLParser( intf_thread_t *pIntf, const string &rFileName ):
    SkinObject( pIntf )
{
    m_pReader = xmlNewTextReaderFilename( rFileName.c_str() );
    if( !m_pReader )
    {
        msg_Err( getIntf(), "Failed to open %s for parsing",
                 rFileName.c_str() );
        return;
    }

    // Activate DTD validation
    xmlTextReaderSetParserProp( m_pReader, XML_PARSER_DEFAULTATTRS, 1 );
    xmlTextReaderSetParserProp( m_pReader, XML_PARSER_VALIDATE, 1 );

    // Set the error handler
    xmlTextReaderSetErrorHandler( m_pReader, handleError, this );
}


XMLParser::~XMLParser()
{
    if( m_pReader )
    {
        xmlFreeTextReader( m_pReader );
    }
}


bool XMLParser::parse()
{
    if( !m_pReader )
    {
        return false;
    }

    m_errors = false;

    int ret = xmlTextReaderRead( m_pReader );
    while (ret == 1)
    {
        if( m_errors )
        {
            return false;
        }
        // Get the node type
        int type = xmlTextReaderNodeType( m_pReader );
        switch (type )
        {
            // Error
            case -1:
                return false;
                break;

            // Begin element
            case 1:
            {
                // Read the element name
                const xmlChar *eltName = xmlTextReaderConstName( m_pReader );
                if( !eltName )
                {
                    return false;
                }
                // Read the attributes
                AttrList_t attributes;
                while( xmlTextReaderMoveToNextAttribute( m_pReader ) == 1 )
                {
                    const xmlChar *name = xmlTextReaderConstName( m_pReader );
                    const xmlChar *value = xmlTextReaderConstValue( m_pReader );
                    if( !name || !value )
                    {
                        return false;
                    }
                    attributes[(const char*)name] = (const char*)value;
                }
                handleBeginElement( (const char*)eltName, attributes);
                break;
            }

            // End element
            case 15:
                // Read the element name
                const xmlChar *eltName = xmlTextReaderConstName( m_pReader );
                if( !eltName )
                {
                    return false;
                }
                handleEndElement( (const char*)eltName );
                break;
        }
        ret = xmlTextReaderRead( m_pReader );
    }
    return (ret == 0 && !m_errors );
}


void XMLParser::handleError( void *pArg,  const char *pMsg,
                             xmlParserSeverities severity,
                             xmlTextReaderLocatorPtr locator)
{
    XMLParser *pThis = (XMLParser*)pArg;
    int line = xmlTextReaderLocatorLineNumber( locator );
    msg_Err( pThis->getIntf(), "XML parser error (line %d) : %s", line, pMsg );
    pThis->m_errors = true;
}
 
