/*****************************************************************************
 * xmlparser.cpp
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: xmlparser.cpp,v 1.2 2004/01/24 14:25:16 asmax Exp $
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

#include "xmlparser.hpp"

XMLParser::XMLParser( intf_thread_t *pIntf, const string &rFileName ):
    SkinObject( pIntf )
{
    m_pReader = xmlNewTextReaderFilename( rFileName.c_str() );
    if( !m_pReader )
    {
        msg_Err( getIntf(), "Failed to open %s for parsing",
                 rFileName.c_str() );
    }

    xmlTextReaderSetParserProp( m_pReader, XML_PARSER_VALIDATE, 1 );
}


XMLParser::~XMLParser()
{
    if( m_pReader )
    {
        xmlFreeTextReader( m_pReader );
    }
}


int XMLParser::parse()
{
    if( !m_pReader )
    {
        return -1;
    }

    int ret = xmlTextReaderRead( m_pReader );
    while (ret == 1)
    {
        // Get the node type
        int type = xmlTextReaderNodeType( m_pReader );
        switch (type )
        {
            // Error
            case -1:
                return -1;
                break;

            // Begin element
            case 1:
            {
                // Read the element name
                const xmlChar *eltName = xmlTextReaderConstName( m_pReader );
                if( !eltName )
                {
                    return -1;
                }
                // Read the attributes
                AttrList_t attributes;
                while( xmlTextReaderMoveToNextAttribute( m_pReader ) == 1 )
                {
                    const xmlChar *name = xmlTextReaderConstName( m_pReader );
                    const xmlChar *value = xmlTextReaderConstValue( m_pReader );
                    if( !name || !value )
                    {
                        return -1;
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
                    return -1;
                }
                handleEndElement( (const char*)eltName );
                break;
        }
        ret = xmlTextReaderRead( m_pReader );
    }
    return 0;
}


void XMLParser::handleBeginElement( const string &rName,
                                    AttrList_t &attributes )
{
    fprintf(stderr,"%s\n", rName.c_str());
    AttrList_t::const_iterator it;
    for (it = attributes.begin(); it != attributes.end(); it++)
    {
        fprintf(stderr,"  %s = %s\n", (*it).first, (*it).second);
    }
}


void XMLParser::handleEndElement( const string &rName )
{
    fprintf(stderr,"--> %s\n", rName.c_str());
} 
