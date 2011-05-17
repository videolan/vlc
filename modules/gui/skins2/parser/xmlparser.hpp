/*****************************************************************************
 * xmlparser.hpp
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

#ifndef XMLPARSER_HPP
#define XMLPARSER_HPP

#include "../src/skin_common.hpp"
#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <map>

// Current DTD version
#define SKINS_DTD_VERSION "2.0"

/// XML parser using libxml2 text reader API
class XMLParser: public SkinObject
{
public:
    XMLParser( intf_thread_t *pIntf, const string &rFileName );
    virtual ~XMLParser();

    /// Parse the file. Returns true on success
    bool parse();

protected:
    // Key comparison function for type "const char*"
    struct ltstr
    {
        bool operator()(const char* s1, const char* s2) const
        {
            return strcmp(s1, s2) < 0;
        }
    };
    /// Type for attribute lists
    typedef map<const char*, const char*, ltstr> AttrList_t;

    /// Flag for validation errors
    bool m_errors;

    /// Callbacks
    virtual void handleBeginElement( const string &rName, AttrList_t &attr )
        { (void)rName; (void)attr; }
    virtual void handleEndElement( const string &rName ) { (void)rName; }

private:
    void LoadCatalog();

    /// Reader context
    xml_t *m_pXML;
    xml_reader_t *m_pReader;
    stream_t *m_pStream;
};

#endif
